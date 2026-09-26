#ifndef BLINK_HPP
#define BLINK_HPP

//External imports
#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    // Initialize the built-in LED pin as an output
    pinMode(2, OUTPUT);
}

void loop() {
    // Turn the LED on
    digitalWrite(2, HIGH);
    // Wait for 1000 milliseconds (1 second)
    delay(1000);
    // Turn the LED off
    digitalWrite(2, LOW);
    // Wait for 1000 milliseconds (1 second)
    delay(1000);
}

#endif

