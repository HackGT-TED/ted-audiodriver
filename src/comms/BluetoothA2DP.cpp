#include "BluetoothA2DP.h"

BluetoothA2DP* BluetoothA2DP::activeInstance = nullptr;

BluetoothA2DP::BluetoothA2DP(const char* deviceName,
                                                         uint8_t bckPin,
                                                         uint8_t lrcPin,
                                                         uint8_t dataPin)
        : i2s(),
            sink(i2s),
            btDeviceName(deviceName),
            i2sBckPin(bckPin),
            i2sLrcPin(lrcPin),
            i2sDataPin(dataPin),
      lastLogMs(0),
      rxChunkCount(0),
      rxBytesTotal(0),
      lastRxMs(0),
      prevChunks(0),
    prevBytes(0),
    audioDataCallback(nullptr) {}

void BluetoothA2DP::setAudioDataCallback(AudioDataCallback callback) {
    audioDataCallback = callback;
}

void BluetoothA2DP::begin() {
    activeInstance = this;

    i2s.setPins(i2sBckPin, i2sLrcPin, i2sDataPin);
    if (!i2s.begin(I2S_MODE_STD, 44100, I2S_DATA_BIT_WIDTH_16BIT,
                   I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
        Serial.println("[A2DP Test] Failed to initialize I2S output.");
        while (true) {
            delay(1000);
        }
    }

    Serial.println("[A2DP Test] Initializing Bluetooth A2DP sink...");
    sink.set_on_connection_state_changed(onConnectionStateChangedStatic, this);
    sink.set_on_audio_state_changed(onAudioStateChangedStatic, this);
    sink.set_stream_reader(onStreamDataStatic, true);
    sink.set_auto_reconnect(true);
    sink.start(btDeviceName);

    Serial.print("[A2DP Test] Started. Pair to '");
    Serial.print(btDeviceName);
    Serial.println("' and start playback.");
}

void BluetoothA2DP::update() {
    if (millis() - lastLogMs < 2000) {
        return;
    }

    lastLogMs = millis();

    uint32_t chunks = rxChunkCount;
    uint32_t bytes = rxBytesTotal;
    uint32_t deltaChunks = chunks - prevChunks;
    uint32_t deltaBytes = bytes - prevBytes;
    prevChunks = chunks;
    prevBytes = bytes;

    Serial.print("[A2DP Test] RX stats - chunks: ");
    Serial.print(deltaChunks);
    Serial.print(" (total: ");
    Serial.print(chunks);
    Serial.print("), bytes: ");
    Serial.print(deltaBytes);
    Serial.print(" (total: ");
    Serial.print(bytes);
    Serial.println(")");

    if (lastRxMs == 0) {
        Serial.println("[A2DP Test] No audio data received yet. Pair and start playback from phone.");
    } else if ((millis() - lastRxMs) > 5000) {
        Serial.println("[A2DP Test] Audio stream appears idle (no data for >5s).");
    }
}

const char* BluetoothA2DP::connectionStateToString(esp_a2d_connection_state_t state) {
    switch (state) {
        case ESP_A2D_CONNECTION_STATE_DISCONNECTED: return "DISCONNECTED";
        case ESP_A2D_CONNECTION_STATE_CONNECTING: return "CONNECTING";
        case ESP_A2D_CONNECTION_STATE_CONNECTED: return "CONNECTED";
        case ESP_A2D_CONNECTION_STATE_DISCONNECTING: return "DISCONNECTING";
        default: return "UNKNOWN";
    }
}

const char* BluetoothA2DP::audioStateToString(esp_a2d_audio_state_t state) {
    if (state == ESP_A2D_AUDIO_STATE_STARTED) {
        return "STARTED";
    }
    if (state == ESP_A2D_AUDIO_STATE_STOPPED) {
        return "STOPPED/REMOTE_SUSPEND";
    }
    return "UNKNOWN";
}

void BluetoothA2DP::onConnectionStateChangedStatic(esp_a2d_connection_state_t state, void* obj) {
    if (obj != nullptr) {
        static_cast<BluetoothA2DP*>(obj)->onConnectionStateChanged(state);
    }
}

void BluetoothA2DP::onAudioStateChangedStatic(esp_a2d_audio_state_t state, void* obj) {
    if (obj != nullptr) {
        static_cast<BluetoothA2DP*>(obj)->onAudioStateChanged(state);
    }
}

void BluetoothA2DP::onStreamDataStatic(const uint8_t* data, uint32_t length) {
    if (activeInstance != nullptr) {
        activeInstance->onStreamData(data, length);
    }
}

void BluetoothA2DP::onConnectionStateChanged(esp_a2d_connection_state_t state) {
    Serial.print("[A2DP Test] Connection state: ");
    Serial.println(connectionStateToString(state));

    if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
        esp_bd_addr_t* lastPeer = sink.get_last_peer_address();
        if (lastPeer != nullptr) {
            Serial.print("[A2DP Test] Connected peer: ");
            Serial.println(sink.to_str(*lastPeer));
        }
    }
}

void BluetoothA2DP::onAudioStateChanged(esp_a2d_audio_state_t state) {
    Serial.print("[A2DP Test] Audio state: ");
    Serial.println(audioStateToString(state));
}

void BluetoothA2DP::onStreamData(const uint8_t* data, uint32_t length) {
    rxChunkCount++;
    rxBytesTotal += length;
    lastRxMs = millis();

    if (audioDataCallback != nullptr) {
        audioDataCallback(data, length);
    }
}