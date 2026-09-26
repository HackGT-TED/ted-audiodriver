//////////////////////////////////////////////////////////////
// Note: uncomment the following line to enable integration testing
// This will include an hpp file for testing purposes
// Be sure to comment out this line for production builds
//////////////////////////////////////////////////////////////
//#define INTEGRATION_TESTING

#ifdef INTEGRATION_TESTING
#include <Arduino.h>
#include "Songbird/SongbirdCore.h"
#include "Songbird/SongbirdUART.h"
#include "../integration/bluetooth_a2dp_test.hpp" // Testing file to run

//////////////////////////////////////////////////////////////
#else

#include <Arduino.h>
#include <cstring>
#include <SoftwareSerial.h>

#include "Songbird/SongbirdCore.h"
#include "Songbird/SongbirdUART.h"
#include "comms/BluetoothA2DP.h"

#define SERIAL_BAUD 115200
#define COMMS_BAUD 38400
#define AMPLITUDE_PACKET_HEADER 0x10

static SoftwareSerial songbirdSerial(12, 13);
static SongbirdUART uart("Bluetooth Amplitude", songbirdSerial);
static BluetoothA2DP bluetoothA2DP("TED-A2DP-Test");
static std::shared_ptr<SongbirdCore> protocol;

static volatile float latestLeftAmplitude = 0.0f;
static volatile float latestRightAmplitude = 0.0f;
static volatile bool amplitudeReady = false;
float audio2hapticsGain = 2000.f; // Gain factor for audio to haptics conversion

static void onBluetoothAudioData(const uint8_t* data, uint32_t length) {
	const uint32_t frameCount = length / (sizeof(int16_t) * 2);
	if (frameCount == 0) {
		return;
	}

	int32_t leftPeak = 0;
	int32_t rightPeak = 0;

	for (uint32_t frame = 0; frame < frameCount; ++frame) {
		int16_t leftSample;
		int16_t rightSample;
		const uint8_t* frameData = data + frame * sizeof(int16_t) * 2;

		std::memcpy(&leftSample, frameData, sizeof(leftSample));
		std::memcpy(&rightSample, frameData + sizeof(leftSample), sizeof(rightSample));

		const int32_t leftMagnitude = leftSample < 0
			? -static_cast<int32_t>(leftSample)
			: static_cast<int32_t>(leftSample);
		const int32_t rightMagnitude = rightSample < 0
			? -static_cast<int32_t>(rightSample)
			: static_cast<int32_t>(rightSample);

		if (leftMagnitude > leftPeak) {
			leftPeak = leftMagnitude;
		}
		if (rightMagnitude > rightPeak) {
			rightPeak = rightMagnitude;
		}
	}

	latestLeftAmplitude = static_cast<float>(leftPeak) / 32768.0f;
	latestRightAmplitude = static_cast<float>(rightPeak) / 32768.0f;
	amplitudeReady = true;
}

void setup() {
	// Initialize serial debug
    Serial.begin(SERIAL_BAUD);
    delay(2000);
    Serial.println("[Bluetooth 2 Haptics] UART Master Mode...");

	// Get protocol object
  	protocol = uart.getProtocol();
  
  	// Initialize the UART node
  	if (!uart.begin(COMMS_BAUD)) {
      	Serial.println("[Comms Test] Failed to initialize UART node.");
      	while (true) {
          	delay(1000);
      	}
  	}

  	bluetoothA2DP.setAudioDataCallback(onBluetoothAudioData);
  	bluetoothA2DP.begin();
}

void loop() {
  	bluetoothA2DP.update();

  	if (!amplitudeReady) {
    	return;
  	}

  	const float leftAmplitude = latestLeftAmplitude;
  	const float rightAmplitude = latestRightAmplitude;
  	amplitudeReady = false;

	SongbirdCore::Packet packet = protocol->createPacket(AMPLITUDE_PACKET_HEADER);
	float hapticSignal = (leftAmplitude + rightAmplitude) / 2.0f * audio2hapticsGain;
	packet.writeFloat(hapticSignal);
	Serial.print("[Bluetooth 2 Haptics] Sending haptic signal: ");
	Serial.println(hapticSignal, 6);
	protocol->sendPacket(packet);
}

#endif // INTEGRATION_TESTING