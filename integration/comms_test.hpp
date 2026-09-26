#ifndef COMMS_TEST_HPP
#define COMMS_TEST_HPP

#include <Arduino.h>
#include <SoftwareSerial.h>
#include "SongbirdCore.h"
#include "SongbirdUART.h"

#define SERIAL_BAUD 115200
#define COMMS_BAUD 38400

//Serial node object with software serial on pins 26 (RX) and 27 (TX)
SoftwareSerial serial(26, 27); 
SongbirdUART uart("UART Node", serial);
//Serial protocol object
std::shared_ptr<SongbirdCore> core;

void setup() {
    // Initialize serial debug
    Serial.begin(SERIAL_BAUD);
    delay(2000);
    Serial.println("[Comms Test] UART Master Test...");

    // Get the protocol object
    core = uart.getProtocol();

    // Initialize the UART node
    if (!uart.begin(COMMS_BAUD)) {
        Serial.println("[Comms Test] Failed to initialize UART node.");
        while (true) {
            delay(1000);
        }
    }

    Serial.println("[Comms Test] UART node and protocol initialized successfully.");
}

uint16_t loopCounter = 0;

void loop() {
    // Update the UART node to read incoming data and process it
    uart.updateData();

    // Send intermittent packets
    if (loopCounter % 1000 == 0) {
        //serial.println("test");
        auto pkt = core->createPacket(0x10);
        float message = 0.5f; // Example float value to send
        pkt.writeFloat(message);
        core->sendPacket(pkt);
        Serial.print("[Comms Test] Sent message with float value: ");
        Serial.println(message, 6);
    }
    loopCounter++;
}

    #endif // COMMS_TEST_HPP