#ifndef BLUETOOTH_A2DP_H
#define BLUETOOTH_A2DP_H

#include <Arduino.h>
#include <BluetoothA2DPSink.h>

class BluetoothA2DP {
public:
    using AudioDataCallback = void (*)(const uint8_t* data, uint32_t length);

    explicit BluetoothA2DP(const char* deviceName = "TED-A2DP-Test");

    void begin();
    void update();
    void setAudioDataCallback(AudioDataCallback callback);

private:
    static BluetoothA2DP* activeInstance;

    BluetoothA2DPSink sink;
    const char* btDeviceName;
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