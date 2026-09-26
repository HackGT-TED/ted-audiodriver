//////////////////////////////////////////////////////////////
// Note: uncomment the following line to enable integration testing
// This will include an hpp file for testing purposes
// Be sure to comment out this line for production builds
//////////////////////////////////////////////////////////////
#define INTEGRATION_TESTING

#ifdef INTEGRATION_TESTING
#include <Arduino.h>
#include "Songbird/SongbirdCore.h"
#include "Songbird/SongbirdUART.h"
#include "../integration/comms_test.hpp" // Testing file to run

//////////////////////////////////////////////////////////////
#else

#include <Arduino.h>

// put function declarations here:
int myFunction(int, int);

void setup() {

}

void loop() {

}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}

#endif // INTEGRATION_TESTING