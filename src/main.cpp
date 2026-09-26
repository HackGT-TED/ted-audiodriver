//////////////////////////////////////////////////////////////
// Note: uncomment the following line to enable integration testing
// This will include an hpp file for testing purposes
// Be sure to comment out this line for production builds
//////////////////////////////////////////////////////////////
#define INTEGRATION_TESTING

#ifdef INTEGRATION_TESTING
#include "../integration/bluetooth_a2dp_test.hpp" // Testing file to run
#endif
//////////////////////////////////////////////////////////////


#include <Arduino.h>

// put function declarations here:
int myFunction(int, int);

void setup() {
#ifdef INTEGRATION_TESTING
  setupBluetoothA2DPTest();
#else
  // put your setup code here, to run once:
  int result = myFunction(2, 3);
  (void)result;
#endif
}

void loop() {
#ifdef INTEGRATION_TESTING
  loopBluetoothA2DPTest();
#else
  // put your main code here, to run repeatedly:
#endif
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}