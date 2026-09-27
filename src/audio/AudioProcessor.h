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
        uint16_t fftSize;
        uint8_t queueDepth;
        uint32_t taskStackSize;
        UBaseType_t taskPriority;
        BaseType_t taskCore;
    };

    using MotorCommandCallback = void (*)(float amplitude);

    AudioProcessor(const Config& config, MotorCommandCallback callback);

    bool begin();
    void onAudioData(const uint8_t* data, uint32_t length);

private:
    static constexpr uint32_t MAX_AUDIO_CHUNK_BYTES = 2048;
    static constexpr uint16_t MAX_FFT_SIZE = 1024;

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
    double samples[MAX_FFT_SIZE];
    uint16_t sampleCount;
    double realSpectrum[MAX_FFT_SIZE];
    double imaginarySpectrum[MAX_FFT_SIZE];
};

#endif
