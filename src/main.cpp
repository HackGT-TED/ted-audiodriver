//////////////////////////////////////////////////////////////
// Note: uncomment the following line to enable integration testing
// This will include an hpp file for testing purposes
// Be sure to comment out this line for production builds
//////////////////////////////////////////////////////////////
//#define INTEGRATION_TESTING

#ifdef INTEGRATION_TESTING
#include <Arduino.h>
#include "SongbirdCore.h"
#include "SongbirdUART.h"
#include "../integration/foc_motor_standalone_test.hpp" // Testing file to run

//////////////////////////////////////////////////////////////
#else

#include <Arduino.h>
#define ENABLE_MOTOR_COMMANDS 1

#if ENABLE_MOTOR_COMMANDS
#include <SoftwareSerial.h>
#endif

#if ENABLE_MOTOR_COMMANDS
#include "SongbirdCore.h"
#include "SongbirdUART.h"
#endif
#include "comms/BluetoothA2DP.h"
#include "audio/AudioProcessor.h"

#define SERIAL_BAUD 115200
#define COMMS_BAUD 38400
#define COMMAND_PACKET_HEADER 0x10
#define AUDIO_SAMPLE_RATE 44100.0f
#define AUDIO_DOWNSAMPLE_FACTOR 22.0f
#define HAPTIC_MIN_FREQUENCY 0.5f
#define HAPTIC_MAX_FREQUENCY 2.0f
#define AUDIO_IDLE_TIMEOUT_MS 100

// I2S pins
#define I2S_BCK_PIN 14
#define I2S_LRC_PIN 25
#define I2S_DATA_PIN 22

#if ENABLE_MOTOR_COMMANDS
static SoftwareSerial songbirdSerial(26, 27);
static SongbirdUART uart("Bluetooth Amplitude", songbirdSerial);
#endif
static BluetoothA2DP bluetoothA2DP("TED-A2DP-Test", I2S_BCK_PIN, I2S_LRC_PIN, I2S_DATA_PIN);
#if ENABLE_MOTOR_COMMANDS
static std::shared_ptr<SongbirdCore> protocol;
#endif

static volatile float latestHapticPhase = 0.0f;
static volatile float latestHapticFrequency = 0.0f;
static volatile bool amplitudeReady = false;
static volatile uint32_t lastAudioDataMs = 0;
static bool zeroSignalSent = false;
static uint32_t lastProcessorLogMs = 0;
static uint32_t lastLoggedWindowCount = 0;

static void onMotorCommand(float phase, float frequency) {
	latestHapticPhase = phase;
	latestHapticFrequency = frequency;
	amplitudeReady = true;
}

static AudioProcessor audioProcessor({
	AUDIO_SAMPLE_RATE / AUDIO_DOWNSAMPLE_FACTOR,
	HAPTIC_MIN_FREQUENCY,
	HAPTIC_MAX_FREQUENCY,
	0.0f,
	1.0f,
	0.5f,
	4096,
	1,
	2048,
	1,
	1
}, onMotorCommand);

static void onBluetoothAudioData(const uint8_t* data, uint32_t length) {
	lastAudioDataMs = millis();
	zeroSignalSent = false;
	audioProcessor.onAudioData(data, length);
}

#if ENABLE_MOTOR_COMMANDS
static void sendMotorCommand(float phase, float frequency) {
	SongbirdCore::Packet packet = protocol->createPacket(COMMAND_PACKET_HEADER);
	packet.writeFloat(phase);
	packet.writeFloat(frequency);
	protocol->sendPacket(packet);
}
#endif

void setup() {
	// Initialize serial debug
    Serial.begin(SERIAL_BAUD);
    delay(2000);
    Serial.println("[Bluetooth 2 Haptics] UART Master Mode...");

	#if ENABLE_MOTOR_COMMANDS
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

	if (!audioProcessor.begin()) {
		Serial.println("[Bluetooth 2 Haptics] Failed to start audio analysis task.");
		while (true) {
			delay(1000);
		}
	}

  	bluetoothA2DP.setAudioDataCallback(onBluetoothAudioData);
	bluetoothA2DP.begin();

	#if ENABLE_MOTOR_COMMANDS
	sendMotorCommand(0.0f, 0.0f);
	zeroSignalSent = true;
	#endif
}

void loop() {
  	bluetoothA2DP.update();
	if (millis() - lastProcessorLogMs >= 2000) {
		lastProcessorLogMs = millis();
		const uint32_t windowCount = audioProcessor.getProcessedWindowCount();
		Serial.print("[Audio Processor] chunksize: ");
		Serial.print(audioProcessor.getLastChunkLength());
		Serial.print(" FFT windows: ");
		Serial.println(windowCount - lastLoggedWindowCount);
		lastLoggedWindowCount = windowCount;
	}

	#if ENABLE_MOTOR_COMMANDS
	const bool audioIdle = lastAudioDataMs == 0 ||
		(millis() - lastAudioDataMs) > AUDIO_IDLE_TIMEOUT_MS;
	if (audioIdle) {
		amplitudeReady = false;
		if (!zeroSignalSent) {
			sendMotorCommand(0.0f, 0.0f);
			zeroSignalSent = true;
			Serial.println("[Bluetooth 2 Haptics] Sending zero: audio idle");
		}
		return;
	}

	if (!amplitudeReady) {
    	return;
  	}

	const float hapticPhase = latestHapticPhase;
	const float hapticFrequency = latestHapticFrequency;
  	amplitudeReady = false;
	
	sendMotorCommand(hapticPhase, hapticFrequency);
	Serial.print("[Bluetooth 2 Haptics] Sending phase: ");
	Serial.print(hapticPhase, 4);
	Serial.print(" frequency: ");
	Serial.print(hapticFrequency, 3);
	Serial.println();
	#endif
}

#endif // INTEGRATION_TESTING