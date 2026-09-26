#ifndef BLUETOOTH_A2DP_TEST_HPP
#define BLUETOOTH_A2DP_TEST_HPP

#include <Arduino.h>
#include <ESP_I2S.h>

#include <BluetoothA2DPSink.h>

// I2S output pin map: BCLK, LRCLK/WS, DATA.
static constexpr uint8_t A2DP_I2S_PIN_BCK = 26;
static constexpr uint8_t A2DP_I2S_PIN_WS = 25;
static constexpr uint8_t A2DP_I2S_PIN_DATA = 22;

static I2SClass i2s;
static BluetoothA2DPSink a2dpSink(i2s);
static unsigned long lastLogMs = 0;

inline void setupBluetoothA2DPTest() {
    Serial.begin(115200);
    delay(1000);

    i2s.setPins(A2DP_I2S_PIN_BCK, A2DP_I2S_PIN_WS, A2DP_I2S_PIN_DATA);
    if (!i2s.begin(I2S_MODE_STD, 44100, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
        Serial.println("[A2DP Test] Failed to initialize I2S.");
        while (true) {
            delay(1000);
        }
    }

    Serial.println("[A2DP Test] Initializing Bluetooth A2DP sink...");
    a2dpSink.set_auto_reconnect(true);
    a2dpSink.start("TED-A2DP-Test");
    Serial.println("[A2DP Test] Started. Pair and stream audio to 'TED-A2DP-Test'.");
}

inline void loopBluetoothA2DPTest() {
    if (millis() - lastLogMs >= 5000) {
        lastLogMs = millis();
        Serial.println("[A2DP Test] Waiting for or receiving stream...");
    }
}

#endif // BLUETOOTH_A2DP_TEST_HPP
