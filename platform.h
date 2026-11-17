/* Settings the whole build needs, not just the sketch, so they cannot go in kopernik-soil.ino -
 * the library's own files read them too. Arduino IDE only: PlatformIO ignores this file and uses
 * the same settings from platformio.ini.
 */
#ifndef PLATFORM_H
#define PLATFORM_H
#define SYSTEM_LITTLEFS_SUPPORTDEPRECATED   // Reads the older config file format too
#define CONFIG_ASYNC_TCP_STACK_SIZE 4096    // The captive portal's web server uses this
#define SYSTEM_OTA_PREFIX "kopernik_soil"   // Which firmware over-the-air update looks for
#if defined(ARDUINO_ESP8266_WEMOS_D1MINI)
  #define SYSTEM_OTA_SUFFIX "d1_mini"
#elif defined(ARDUINO_NOLOGO_ESP32C3_SUPER_MINI)
  #define SYSTEM_OTA_SUFFIX "supermini"
#endif
#define CONTROL_HTTPPOST_DEBUG              // Uncomment to debug HTTP posts
#define SENSOR_SOIL_DEBUG                   // Uncomment to debug SOIL readings
#define SYSTEM_WIFI_DEBUG                   // Uncomment to debug WIFI connection attempts
#define SYSTEM_MQTT_DEBUG                   // Uncomment to debug MQTT interactions
#endif // PLATFORM_H
