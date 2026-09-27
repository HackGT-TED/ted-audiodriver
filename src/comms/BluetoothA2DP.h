#ifndef BLUETOOTH_A2DP_H
#define BLUETOOTH_A2DP_H

#include <Arduino.h>
#include <ESP_I2S.h>
#include <BluetoothA2DPSinkQueued.h>

class GuardedBluetoothA2DPSinkQueued : public BluetoothA2DPSinkQueued {
public:
    explicit GuardedBluetoothA2DPSinkQueued(Print& output)
        : BluetoothA2DPSinkQueued(output) {}

protected:
    size_t write_audio(const uint8_t* data, size_t size) override {
        if (s_ringbuf_i2s == nullptr || s_i2s_write_semaphore == nullptr) {
            return 0;
        }
        return BluetoothA2DPSinkQueued::write_audio(data, size);
    }
};

class BluetoothA2DP {
public:
    using AudioDataCallback = void (*)(const uint8_t* data, uint32_t length);

    explicit BluetoothA2DP(const char* deviceName = "TED-A2DP-Test",
                           uint8_t bckPin = 26,
                           uint8_t lrcPin = 25,
                           uint8_t dataPin = 22);

    void begin();
    void update();
    void setAudioDataCallback(AudioDataCallback callback);

private:
    static BluetoothA2DP* activeInstance;

    I2SClass i2s;
    GuardedBluetoothA2DPSinkQueued sink;
    const char* btDeviceName;
    uint8_t i2sBckPin;
    uint8_t i2sLrcPin;
    uint8_t i2sDataPin;
    unsigned long lastLogMs;
    uint32_t rxChunkCount;
    uint32_t rxBytesTotal;
    unsigned long lastRxMs;
    uint32_t prevChunks;
    uint32_t prevBytes;
    AudioDataCallback audioDataCallback;

    static const char* connectionStateToString(esp_a2d_connection_state_t state);
    static const char* audioStateToString(esp_a2d_audio_state_t state);

    static void onConnectionStateChangedStatic(esp_a2d_connection_state_t state, void* obj);
    static void onAudioStateChangedStatic(esp_a2d_audio_state_t state, void* obj);
    static void onStreamDataStatic(const uint8_t* data, uint32_t length);

    void onConnectionStateChanged(esp_a2d_connection_state_t state);
    void onAudioStateChanged(esp_a2d_audio_state_t state);
    void onStreamData(const uint8_t* data, uint32_t length);
};

#endif // BLUETOOTH_A2DP_H