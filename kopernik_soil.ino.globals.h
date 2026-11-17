// ESP8266 only: the core reads this file, found by its exact name, into every file it compiles.
// It just passes platform.h on - ESP32 reads platform.h directly. Keep both beside the .ino.
// ../sketch/ because the core reads a copy of this file from the build folder, where the Arduino
// IDE has also copied the sketch's files into sketch/.
#define FRUGAL_IOT_GLOBALS_FOUND // Tells the library this file reached the build
#include "../sketch/platform.h"
