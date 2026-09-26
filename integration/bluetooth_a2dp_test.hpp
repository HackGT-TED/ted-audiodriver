#ifndef BLUETOOTH_A2DP_TEST_HPP
#define BLUETOOTH_A2DP_TEST_HPP

#include <Arduino.h>

#include <BluetoothA2DPSink.h>

static BluetoothA2DPSink a2dpSink;
static unsigned long lastLogMs = 0;
static volatile uint32_t rxChunkCount = 0;
static volatile uint32_t rxBytesTotal = 0;
static volatile unsigned long lastRxMs = 0;

static const char* connectionStateToString(esp_a2d_connection_state_t state) {
    switch (state) {
        case ESP_A2D_CONNECTION_STATE_DISCONNECTED: return "DISCONNECTED";
        case ESP_A2D_CONNECTION_STATE_CONNECTING: return "CONNECTING";
        case ESP_A2D_CONNECTION_STATE_CONNECTED: return "CONNECTED";
        case ESP_A2D_CONNECTION_STATE_DISCONNECTING: return "DISCONNECTING";
        default: return "UNKNOWN";
    }
}

static const char* audioStateToString(esp_a2d_audio_state_t state) {
    if (state == ESP_A2D_AUDIO_STATE_STARTED) {
        return "STARTED";
    }
    if (state == ESP_A2D_AUDIO_STATE_STOPPED) {
        return "STOPPED/REMOTE_SUSPEND";
    }
    return "UNKNOWN";
}

static void onConnectionStateChanged(esp_a2d_connection_state_t state, void*) {
    Serial.print("[A2DP Test] Connection state: ");
    Serial.println(connectionStateToString(state));

    if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
        esp_bd_addr_t* lastPeer = a2dpSink.get_last_peer_address();
        if (lastPeer != nullptr) {
            Serial.print("[A2DP Test] Connected peer: ");
            Serial.println(a2dpSink.to_str(*lastPeer));
        }
    }
}

static void onAudioStateChanged(esp_a2d_audio_state_t state, void*) {
    Serial.print("[A2DP Test] Audio state: ");
    Serial.println(audioStateToString(state));
}

static void onStreamData(const uint8_t*, uint32_t length) {
    rxChunkCount++;
    rxBytesTotal += length;
    lastRxMs = millis();
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("[A2DP Test] Initializing Bluetooth A2DP sink...");
    a2dpSink.set_on_connection_state_changed(onConnectionStateChanged);
    a2dpSink.set_on_audio_state_changed(onAudioStateChanged);
    a2dpSink.set_stream_reader(onStreamData, false);
    a2dpSink.set_auto_reconnect(true);
    a2dpSink.start("TED-A2DP-Test");
    Serial.println("[A2DP Test] Started. Pair to 'TED-A2DP-Test' and start playback.");
}

void loop() {
    static uint32_t prevChunks = 0;
    static uint32_t prevBytes = 0;

    if (millis() - lastLogMs >= 2000) {
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
}

#endif // BLUETOOTH_A2DP_TEST_HPP
