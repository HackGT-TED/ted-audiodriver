#include "AudioProcessor.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <ArduinoFFT.h>

AudioProcessor::AudioProcessor(const Config& config, MotorCommandCallback callback)
    : config(config),
      callback(callback),
      queue(nullptr),
      samples{},
      sampleCount(0),
      realSpectrum{},
      imaginarySpectrum{} {}

bool AudioProcessor::begin() {
    if (config.fftSize == 0 || config.fftSize > MAX_FFT_SIZE ||
        (config.fftSize & (config.fftSize - 1)) != 0) {
        return false;
    }

    queue = xQueueCreate(config.queueDepth, sizeof(AudioChunk));
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

    AudioChunk chunk;
    chunk.length = length;
    std::memcpy(chunk.data, data, length);
    xQueueSend(queue, &chunk, 0);
}

void AudioProcessor::taskEntry(void* context) {
    static_cast<AudioProcessor*>(context)->taskLoop();
}

void AudioProcessor::taskLoop() {
    AudioChunk chunk;
    while (true) {
        if (xQueueReceive(queue, &chunk, portMAX_DELAY) == pdTRUE) {
            processChunk(chunk);
        }
    }
}

void AudioProcessor::processChunk(const AudioChunk& chunk) {
    const uint32_t frameCount = chunk.length / (sizeof(int16_t) * 2);
    for (uint32_t frame = 0; frame < frameCount; ++frame) {
        int16_t leftSample;
        int16_t rightSample;
        const uint8_t* frameData = chunk.data + frame * sizeof(int16_t) * 2;
        std::memcpy(&leftSample, frameData, sizeof(leftSample));
        std::memcpy(&rightSample, frameData + sizeof(leftSample), sizeof(rightSample));

        samples[sampleCount++] = (static_cast<double>(leftSample) + rightSample) * 0.5;
        if (sampleCount == config.fftSize) {
            processWindow();
            const uint16_t overlap = config.fftSize / 2;
            std::memmove(samples, samples + overlap,
                         (config.fftSize - overlap) * sizeof(samples[0]));
            sampleCount = config.fftSize - overlap;
        }
    }
}

void AudioProcessor::processWindow() {
    // Performs FFT on the current window of samples
    std::fill(realSpectrum, realSpectrum + config.fftSize, 0.0);
    std::fill(imaginarySpectrum, imaginarySpectrum + config.fftSize, 0.0);
    std::memcpy(realSpectrum, samples, config.fftSize * sizeof(samples[0]));

    ArduinoFFT<double> fft(realSpectrum, imaginarySpectrum, config.fftSize,
                           config.sampleRate);
    fft.windowing(FFTWindow::Hamming, FFTDirection::Forward);
    fft.compute(FFTDirection::Forward);

    const uint16_t halfSize = config.fftSize / 2;
    const double binWidth = config.sampleRate / config.fftSize;

    // Apply the gain ramp to the complex bins, preserving phase and polarity.
    realSpectrum[0] = 0.0;
    imaginarySpectrum[0] = 0.0;
    for (uint16_t bin = 1; bin < halfSize; ++bin) {
        const double frequency = bin * binWidth;
        double gain = 0.0;
        if (frequency >= config.motorMinFrequency &&
            frequency <= config.motorMaxFrequency) {
            const double position = (frequency - config.motorMinFrequency) /
                                   (config.motorMaxFrequency - config.motorMinFrequency);
            gain = config.lowFrequencyGain +
                   position * (config.highFrequencyGain - config.lowFrequencyGain);
        }

        const uint16_t mirroredBin = config.fftSize - bin;
        realSpectrum[bin] *= gain;
        imaginarySpectrum[bin] *= gain;
        realSpectrum[mirroredBin] = realSpectrum[bin];
        imaginarySpectrum[mirroredBin] = -imaginarySpectrum[bin];
    }
    realSpectrum[halfSize] = 0.0;
    imaginarySpectrum[halfSize] = 0.0;

    fft.compute(FFTDirection::Reverse);

    double peakSample = 0.0;
    for (uint16_t sample = 0; sample < config.fftSize; ++sample) {
        if (std::abs(realSpectrum[sample]) > std::abs(peakSample)) {
            peakSample = realSpectrum[sample];
        }
    }

    if (callback == nullptr) {
        return;
        }

    const float motorAmplitude = static_cast<float>(std::clamp(
        peakSample / 32768.0, -1.0, 1.0));
    callback(motorAmplitude);
}
