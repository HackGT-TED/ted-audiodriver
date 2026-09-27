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
#include "../integration/blink_test.hpp" // Testing file to run

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
#define HAPTIC_MIN_FREQUENCY 40.0f
#define HAPTIC_MAX_FREQUENCY 480.0f
#define HAPTIC_FREQUENCY_STEP 40.0f

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

static volatile float latestHapticAmplitude = 0.0f;
static volatile bool amplitudeReady = false;

static void onMotorCommand(float amplitude) {
	latestHapticAmplitude = amplitude;
	amplitudeReady = true;
}

static AudioProcessor audioProcessor({
	AUDIO_SAMPLE_RATE,
	HAPTIC_MIN_FREQUENCY,
	HAPTIC_MAX_FREQUENCY,
	0.0f,
	1.0f,
	256,
	1,
	4096,
	1,
	1
}, onMotorCommand);

static void onBluetoothAudioData(const uint8_t* data, uint32_t length) {
	audioProcessor.onAudioData(data, length);
}

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
}

void loop() {
  	bluetoothA2DP.update();

	#if ENABLE_MOTOR_COMMANDS
	if (!amplitudeReady) {
    	return;
  	}

	const float hapticAmplitude = latestHapticAmplitude;
  	amplitudeReady = false;

	SongbirdCore::Packet packet = protocol->createPacket(COMMAND_PACKET_HEADER);
	packet.writeFloat(hapticAmplitude);
	Serial.print("[Bluetooth 2 Haptics] Sending amplitude: ");
	Serial.print(hapticAmplitude, 6);
	Serial.println();
	protocol->sendPacket(packet);
	#endif
}

#endif // INTEGRATION_TESTING