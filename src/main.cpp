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
#include <cmath>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#define ENABLE_SONGBIRD_UART 0

#if ENABLE_SONGBIRD_UART
#include <SoftwareSerial.h>
#endif

#if ENABLE_SONGBIRD_UART
#include "SongbirdCore.h"
#include "SongbirdUART.h"
#endif
#include "comms/BluetoothA2DP.h"

#define SERIAL_BAUD 115200
#define COMMS_BAUD 38400
#define COMMAND_PACKET_HEADER 0x10
#define AUDIO_SAMPLE_RATE 44100.0f
#define HAPTIC_MIN_FREQUENCY 40.0f
#define HAPTIC_MAX_FREQUENCY 480.0f
#define HAPTIC_FREQUENCY_STEP 40.0f

// I2S pins
#define I2S_BCK_PIN 14
#define I2S_LRC_PIN 25
#define I2S_DATA_PIN 22

#if ENABLE_SONGBIRD_UART
static SoftwareSerial songbirdSerial(26, 27);
static SongbirdUART uart("Bluetooth Amplitude", songbirdSerial);
#endif
static BluetoothA2DP bluetoothA2DP("TED-A2DP-Test", I2S_BCK_PIN, I2S_LRC_PIN, I2S_DATA_PIN);
#if ENABLE_SONGBIRD_UART
static std::shared_ptr<SongbirdCore> protocol;
#endif

static volatile float latestHapticFrequency = 0.0f;
static volatile float latestHapticAmplitude = 0.0f;
static volatile bool amplitudeReady = false;

constexpr uint32_t AUDIO_QUEUE_DEPTH = 1;
constexpr uint32_t MAX_AUDIO_CHUNK_BYTES = 2048;

struct AudioChunk {
	uint32_t length;
	uint8_t data[MAX_AUDIO_CHUNK_BYTES];
};

static QueueHandle_t audioAnalysisQueue = nullptr;

static void processAudioData(const uint8_t* data, uint32_t length) {
	const uint32_t frameCount = length / (sizeof(int16_t) * 2);
	if (frameCount == 0) {
		return;
	}

	constexpr uint32_t bandCount = static_cast<uint32_t>(
		(HAPTIC_MAX_FREQUENCY - HAPTIC_MIN_FREQUENCY) / HAPTIC_FREQUENCY_STEP) + 1;
	float q1[bandCount] = {};
	float q2[bandCount] = {};
	float coefficients[bandCount];

	for (uint32_t band = 0; band < bandCount; ++band) {
		const float frequency = HAPTIC_MIN_FREQUENCY + band * HAPTIC_FREQUENCY_STEP;
		const float omega = 2.0f * PI * frequency / AUDIO_SAMPLE_RATE;
		coefficients[band] = 2.0f * std::cos(omega);
	}

	for (uint32_t frame = 0; frame < frameCount; ++frame) {
		int16_t leftSample;
		int16_t rightSample;
		const uint8_t* frameData = data + frame * sizeof(int16_t) * 2;

		std::memcpy(&leftSample, frameData, sizeof(leftSample));
		std::memcpy(&rightSample, frameData + sizeof(leftSample), sizeof(rightSample));

		const float monoSample = static_cast<float>(leftSample + rightSample) * 0.5f;
		for (uint32_t band = 0; band < bandCount; ++band) {
			const float q0 = coefficients[band] * q1[band] - q2[band] + monoSample;
			q2[band] = q1[band];
			q1[band] = q0;
		}
	}

	float strongestPower = 0.0f;
	uint32_t strongestBand = 0;
	for (uint32_t band = 0; band < bandCount; ++band) {
		const float power = q1[band] * q1[band] + q2[band] * q2[band]
			- coefficients[band] * q1[band] * q2[band];
		if (power > strongestPower) {
			strongestPower = power;
			strongestBand = band;
		}
	}

	const float lowBandAmplitude = std::min(
		1.0f,
		2.0f * std::sqrt(strongestPower) / (static_cast<float>(frameCount) * 32768.0f));
	latestHapticAmplitude = lowBandAmplitude;
	latestHapticFrequency = HAPTIC_MIN_FREQUENCY + strongestBand * HAPTIC_FREQUENCY_STEP;
	amplitudeReady = true;
}

static void audioAnalysisTask(void*) {
	AudioChunk chunk;
	while (true) {
		if (xQueueReceive(audioAnalysisQueue, &chunk, portMAX_DELAY) == pdTRUE) {
			processAudioData(chunk.data, chunk.length);
		}
	}
}

static void onBluetoothAudioData(const uint8_t* data, uint32_t length) {
	if (audioAnalysisQueue == nullptr || length > MAX_AUDIO_CHUNK_BYTES) {
		return;
	}

	AudioChunk chunk;
	chunk.length = length;
	std::memcpy(chunk.data, data, length);
	// Never block the Bluetooth audio task when analysis falls behind.
	xQueueSend(audioAnalysisQueue, &chunk, 0);
}

void setup() {
	// Initialize serial debug
    Serial.begin(SERIAL_BAUD);
    delay(2000);
    Serial.println("[Bluetooth 2 Haptics] UART Master Mode...");

	#if ENABLE_SONGBIRD_UART
	// Get protocol object
  	protocol = uart.getProtocol();
  
  	// Initialize the UART node
  	if (!uart.begin(COMMS_BAUD)) {
      	Serial.println("[Comms Test] Failed to initialize UART node.");
      	while (true) {
          	delay(1000);
      	}
  	}
	#endif

	audioAnalysisQueue = xQueueCreate(AUDIO_QUEUE_DEPTH, sizeof(AudioChunk));
	if (audioAnalysisQueue == nullptr ||
		xTaskCreatePinnedToCore(audioAnalysisTask, "audio_analysis", 4096,
								 nullptr, 1, nullptr, 1) != pdPASS) {
		Serial.println("[Bluetooth 2 Haptics] Failed to start audio analysis task.");
		while (true) {
			delay(1000);
		}
	}

  	bluetoothA2DP.setAudioDataCallback(onBluetoothAudioData);
	bluetoothA2DP.begin();
}

void loop() {
  	bluetoothA2DP.update();

	#if ENABLE_SONGBIRD_UART
	if (!amplitudeReady) {
    	return;
  	}

	const float hapticAmplitude = latestHapticAmplitude;
	const float hapticFrequency = latestHapticFrequency;
  	amplitudeReady = false;

	SongbirdCore::Packet packet = protocol->createPacket(COMMAND_PACKET_HEADER);
	packet.writeFloat(hapticAmplitude);
	packet.writeFloat(hapticFrequency);
	Serial.print("[Bluetooth 2 Haptics] Sending amplitude: ");
	Serial.print(hapticAmplitude, 6);
	Serial.print(" frequency: ");
	Serial.println(hapticFrequency, 1);
	protocol->sendPacket(packet);
	#endif
}

#endif // INTEGRATION_TESTING