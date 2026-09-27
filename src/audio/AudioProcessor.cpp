#include "AudioProcessor.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <ArduinoFFT.h>
#include "esp_system.h"

static constexpr uint16_t DOWNSAMPLE_FACTOR = 22;

AudioProcessor::AudioProcessor(const Config& config, MotorCommandCallback callback)
    : config(config),
      callback(callback),
      queue(nullptr),
      sampleCount(0),
    downsampleCount(0),
    downsampleAccumulator(0),
    overlapSamples{},
      realSpectrum{},
    imaginarySpectrum{},
    processedWindowCount(0),
    lastChunkLength(0) {}

bool AudioProcessor::begin() {
    if (config.fftSize == 0 || config.fftSize > MAX_FFT_SIZE ||
        (config.fftSize & (config.fftSize - 1)) != 0) {
        return false;
    }

    queue = xQueueCreate(config.queueDepth, sizeof(AudioChunk*));
    if (queue == nullptr) {
        return false;
    }

    return xTaskCreatePinnedToCore(
        taskEntry,
        "audio_fft",
        config.taskStackSize,
        this,
        config.taskPriority,
        nullptr,
        config.taskCore) == pdPASS;
}

void AudioProcessor::onAudioData(const uint8_t* data, uint32_t length) {
    if (queue == nullptr || data == nullptr || length == 0 ||
        length > MAX_AUDIO_CHUNK_BYTES) {
        return;
    }

    lastChunkLength = length;
    AudioChunk* chunk = static_cast<AudioChunk*>(std::malloc(sizeof(AudioChunk)));
    if (chunk == nullptr) {
        return;
    }

    chunk->length = length;
    std::memcpy(chunk->data, data, length);
    if (xQueueSend(queue, &chunk, 0) != pdTRUE) {
        std::free(chunk);
    }
}

uint32_t AudioProcessor::getProcessedWindowCount() const {
    return processedWindowCount;
}

uint32_t AudioProcessor::getLastChunkLength() const {
    return lastChunkLength;
}

void AudioProcessor::taskEntry(void* context) {
    static_cast<AudioProcessor*>(context)->taskLoop();
}

void AudioProcessor::taskLoop() {
    AudioChunk* chunk = nullptr;
    while (true) {
        if (xQueueReceive(queue, &chunk, portMAX_DELAY) == pdTRUE && chunk != nullptr) {
            processChunk(*chunk);
            std::free(chunk);
        }
    }
}

void AudioProcessor::processChunk(const AudioChunk& chunk) {
    const uint32_t frameCount = chunk.length / (sizeof(int16_t) * 2);
    for (uint32_t frame = 0; frame < frameCount; frame = frame + 1) {
        int16_t leftSample;
        int16_t rightSample;
        const uint8_t* frameData = chunk.data + frame * sizeof(int16_t) * 2;
        std::memcpy(&leftSample, frameData, sizeof(leftSample));
        std::memcpy(&rightSample, frameData + sizeof(leftSample), sizeof(rightSample));

        downsampleAccumulator += (static_cast<int32_t>(leftSample) + rightSample) / 2;
        downsampleCount = downsampleCount + 1;
        if (downsampleCount == DOWNSAMPLE_FACTOR) {
            realSpectrum[sampleCount] = static_cast<float>(downsampleAccumulator) /
                                         DOWNSAMPLE_FACTOR;
            sampleCount = sampleCount + 1;
            downsampleCount = 0;
            downsampleAccumulator = 0;

            if (sampleCount == config.fftSize) {
                const uint16_t overlap = config.fftSize / 2;
                std::memcpy(overlapSamples, realSpectrum + overlap,
                            overlap * sizeof(realSpectrum[0]));
                processWindow();
                std::memcpy(realSpectrum, overlapSamples,
                            overlap * sizeof(realSpectrum[0]));
                sampleCount = config.fftSize - overlap;
            }
        }
    }
}

void AudioProcessor::processWindow() {
    processedWindowCount = processedWindowCount + 1;
    // Performs FFT on the current window of samples
    std::fill(imaginarySpectrum, imaginarySpectrum + config.fftSize, 0.0);

    ArduinoFFT<float> fft(realSpectrum, imaginarySpectrum, config.fftSize,
                          config.sampleRate);
    fft.windowing(FFTWindow::Hamming, FFTDirection::Forward);
    fft.compute(FFTDirection::Forward);

    const uint16_t halfSize = config.fftSize / 2;
    const float binWidth = config.sampleRate / config.fftSize;

    float dominantMagnitude = 0.0f;
    float dominantPhase = 0.0f;
    float dominantFrequency = config.motorMinFrequency;
    float totalWeightedMagnitude = 0.0f;

    // Select the dominant frequency after applying the configured gain ramp.
    for (uint16_t bin = 1; bin < halfSize; bin = bin + 1) {
        const float frequency = bin * binWidth;
        if (frequency < config.motorMinFrequency ||
            frequency > config.motorMaxFrequency) {
            continue;
        }

        const float position = (frequency - config.motorMinFrequency) /
                               (config.motorMaxFrequency - config.motorMinFrequency);
        const float gain = config.lowFrequencyGain +
                            position * (config.highFrequencyGain - config.lowFrequencyGain);
        const float magnitude = std::hypot(realSpectrum[bin], imaginarySpectrum[bin]);
        const float weightedMagnitude = magnitude * gain;
        totalWeightedMagnitude += weightedMagnitude;
        if (weightedMagnitude > dominantMagnitude) {
            dominantMagnitude = weightedMagnitude;
            dominantFrequency = frequency;
            dominantPhase = std::atan2(imaginarySpectrum[bin], realSpectrum[bin]);
        }
    }

    if (callback == nullptr) {
        return;
        }

    if (dominantMagnitude <= 1.0f) {
        callback(0.0f, 0.0f);
        return;
    }

    const float randomRoll = static_cast<float>(esp_random()) /
                             static_cast<float>(UINT32_MAX);
    if (config.randomSamplingFactor > randomRoll && totalWeightedMagnitude > 0.0f) {
        const float target = (static_cast<float>(esp_random()) /
                              static_cast<float>(UINT32_MAX)) * totalWeightedMagnitude;
        float cumulativeMagnitude = 0.0f;

        for (uint16_t bin = 1; bin < halfSize; bin = bin + 1) {
            const float frequency = bin * binWidth;
            if (frequency < config.motorMinFrequency ||
                frequency > config.motorMaxFrequency) {
                continue;
            }

            const float position = (frequency - config.motorMinFrequency) /
                                   (config.motorMaxFrequency - config.motorMinFrequency);
            const float gain = config.lowFrequencyGain +
                               position * (config.highFrequencyGain - config.lowFrequencyGain);
            cumulativeMagnitude +=
                std::hypot(realSpectrum[bin], imaginarySpectrum[bin]) * gain;
            if (cumulativeMagnitude >= target) {
                dominantFrequency = frequency;
                dominantPhase = std::atan2(imaginarySpectrum[bin], realSpectrum[bin]);
                break;
            }
        }
    }

    callback(dominantPhase, dominantFrequency);
}
