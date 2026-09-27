#ifndef AUDIO_PROCESSOR_H
#define AUDIO_PROCESSOR_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

class AudioProcessor {
public:
    struct Config {
        float sampleRate;
        float motorMinFrequency;
        float motorMaxFrequency;
        float lowFrequencyGain;
        float highFrequencyGain;
        float randomSamplingFactor;
        uint16_t fftSize;
        uint8_t queueDepth;
        uint32_t taskStackSize;
        UBaseType_t taskPriority;
        BaseType_t taskCore;
    };

    using MotorCommandCallback = void (*)(float phase, float frequency);

    AudioProcessor(const Config& config, MotorCommandCallback callback);

    bool begin();
    void onAudioData(const uint8_t* data, uint32_t length);
    uint32_t getProcessedWindowCount() const;
    uint32_t getLastChunkLength() const;

private:
    static constexpr uint32_t MAX_AUDIO_CHUNK_BYTES = 8192;
    static constexpr uint16_t MAX_FFT_SIZE = 4096;

    struct AudioChunk {
        uint32_t length;
        uint8_t data[MAX_AUDIO_CHUNK_BYTES];
    };

    static void taskEntry(void* context);
    void taskLoop();
    void processChunk(const AudioChunk& chunk);
    void processWindow();

    Config config;
    MotorCommandCallback callback;
    QueueHandle_t queue;
    uint16_t sampleCount;
    uint16_t downsampleCount;
    int32_t downsampleAccumulator;
    float overlapSamples[MAX_FFT_SIZE / 2];
    float realSpectrum[MAX_FFT_SIZE];
    float imaginarySpectrum[MAX_FFT_SIZE];
    volatile uint32_t processedWindowCount;
    volatile uint32_t lastChunkLength;
};

#endif
