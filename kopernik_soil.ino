/* kopernik_soil - a soil probe on a Wemos D1 Mini or ESP32-C3 SuperMini that posts to a local hub
 *
 * The Frugal-IoT version of Kopernik's standalone probe sketch: the same pins, the same dry/wet
 * calibration, the same hub and the same form POST, so the hub needs no change. What the library
 * adds on top: a captive portal to set WiFi and calibrate; readings on the Frugal-IoT dashboard over MQTT,
 * over-the-air updates, and a sleep cycle.
 *
 * Everything you are likely to change is in the configuration block below.
 *
 * Arduino IDE: Tools > Board is "LOLIN(WEMOS) D1 R2 & mini" or "Nologo ESP32C3 Super Mini" (also
 * for the SuperMini Pro). Keep every file in this folder together: platform.h holds the settings
 * the library itself needs, kopernik_soil.ino.globals.h is how an ESP8266 build finds them, and
 * partitions.csv makes the SuperMini build fit in flash. On the SuperMini also set Tools > Partition Scheme to
 * "No OTA (2MB APP/2MB SPIFFS)" - over-the-air updates still work, partitions.csv sets the real
 * layout and the menu only raises the size limit the IDE checks against.
 * PlatformIO ignores those files and uses platformio.ini instead.
 *
 * Wiring for the sleep, D1 Mini only: D0 must be connected to RST, as in the original sketch -
 * that is how the ESP8266 wakes itself from deep sleep. Without it the node sleeps once and never
 * comes back. Disconnect it to upload, if the upload fails with it on. An ESP32 needs no wire.
 *
 * Known, and deliberately not worked around here:
 *  - On the D1 Mini the battery reading is commented out while the board's wiring is sorted out -
 *    it shares A0 with the probe - so the hub gets batt and soc as 0. The SuperMini reads it on 4.
 *  - The hub's network has no route to the MQTT broker, so the dashboard only sees this node when
 *    it is on a network with internet.
 */

// ===== Configuration ===========================================================================
// Before the include, so the library's own defaults give way to these.

// The hub - sent as id=, joined by SSID/password, posted to at the URL
#define CONTROL_HTTPPOST_NODE_ID   "B"
#define CONTROL_HTTPPOST_URL       "http://192.168.4.1/ingest"

// Default WiFi - more can be added through captive portal.
#define SYSTEM_WIFI_SSID           "SoilHub"
#define SYSTEM_WIFI_PASSWORD       "soilhub123"

// The probe and battery pins, per board. The power pin goes HIGH to power the probe, with a 100 ms
// settle (SYSTEM_POWER_ON_DELAY). 
// Dry and wet are initial calibration only - use Tare and Calibrate in the captive portal to set them accurately. 
// The battery is off until its wiring is checked.
// The second name on each #if is for PlatformIO, which does not define the Arduino IDE's board macro
// for every board - see platformio.ini.
#if defined(ARDUINO_ESP8266_WEMOS_D1MINI) || defined(D1_MINI)
  #define SENSOR_SOIL_PIN            A0
  #define SENSOR_SOIL_POWER3v3_PIN   5     // D1
  #define SENSOR_SOIL_DRY            900   // Raw reading in dry air = 0 %   (10-bit ADC, 0-1023)
  #define SENSOR_SOIL_WET            250   // Raw reading in water   = 100 %
  //#define SENSOR_BATTERY_PIN       A0    // Shares A0 with the probe - see above
  //#define SENSOR_BATTERY_VOLTAGE_DIVIDER 3.2 // Battery volts per volt at the pin
#elif defined(ARDUINO_NOLOGO_ESP32C3_SUPER_MINI) || defined(SUPERMINI)
  #define SENSOR_SOIL_PIN            3     // Must be GPIO 0-4, the C3's ADC1 - and not 2, a strapping pin
  #define SENSOR_SOIL_POWER3v3_PIN   5
  #define SENSOR_SOIL_DRY            3600  // 12-bit ADC, 0-4095 - roughly 4x the D1 Mini's, recalibrate
  #define SENSOR_SOIL_WET            1000
  #define SENSOR_BATTERY_PIN         4     // ADC1 too. Avoid 8 and 9: the LED and the BOOT button
  #define SENSOR_BATTERY_VOLTAGE_DIVIDER 2 // 100k + 100k: battery volts per volt at the pin
  //#define SYSTEM_POWER_PANIC_MV      0      // Dont power panic - battery sensor a but unpredictable see issu https://github.com/mitra42/frugal-iot/issues/235
#else
  #error "Under Tools > Board pick LOLIN(WEMOS) D1 R2 & mini, or Nologo ESP32C3 Super Mini"
#endif
#define SENSOR_SOIL_SAMPLES          7     // Median of this many readings

// The cycle
#define SYSTEM_POWER_CYCLE_MS      15000   // 15 s between readings - for testing
//#define SYSTEM_POWER_CYCLE_MS    300000  // 5 min - for the field
#define SYSTEM_POWER_WAKE_MS       10000   // Awake this long each cycle, for the post and the dashboard

// The Frugal-IoT dashboard. The node enrols itself with the secret - from the dashboard's gear
// icon -> Nodes -> Enrolment secret - and then has its own credential. With no secret it is refused
// and listed on that Nodes card for an administrator to approve, which also works.
// Failure to connect to the dashboard will not stop the HTTP posts, providing it connects to WiFi 
#define SYSTEM_FRUGAL_ORG          "kopernik"
#define SYSTEM_FRUGAL_PROJECT      "developers"
#define SYSTEM_MQTT_HOST           "frugaliot.naturalinnovation.org"
#ifndef SYSTEM_MQTT_ENROL_SECRET   // PlatformIO: put it in a *-local.ini instead, see platformio.ini
  #define SYSTEM_MQTT_ENROL_SECRET ""
#endif
// ===============================================================================================

#include "Frugal-IoT.h"

System_Frugal frugal_iot(SYSTEM_FRUGAL_ORG, SYSTEM_FRUGAL_PROJECT, "Kopernik Soil", "Soil probe posting to a local hub");

/* Battery state of charge, from a lithium cell's open-circuit voltage - the table from the
 * standalone sketch. Also the same distance guess from WiFi signal strength.
 */
static uint8_t stateOfCharge(float volts) {
  static const struct { float volts; uint8_t percent; } table[] = {
    {4.20, 100}, {4.15, 95}, {4.10, 90}, {4.00, 80}, {3.92, 70}, {3.86, 60},
    {3.80, 50}, {3.75, 40}, {3.70, 30}, {3.62, 20}, {3.50, 10}, {3.30, 5},
  };
  uint8_t soc = 0;
  for (auto &row : table) {
    if (!soc && (volts >= row.volts)) {
      soc = row.percent;
    }
  }
  return soc;
}
static float distanceMetres(int8_t rssi) {
  return (rssi >= -50) ? 1 : (rssi >= -60) ? 2 : (rssi >= -70) ? 5 : (rssi >= -80) ? 10 : 15;
}

/* The post the hub expects, every cycle: id, vwc (0-100, 1 dp), batt (volts, 2 dp), soc (%),
 * dist (metres, 1 dp) - the same fields as the standalone sketch sends.
 */
class Control_Hub : public Control_HttpPost {
  public:
    IN* vwc;
    IN* batt;
    Control_Hub(Sensor_Soil* soil, Sensor_Battery* battery)
      : Control_HttpPost("hub", "Soil hub", CONTROL_HTTPPOST_URL, true) { // true - post every cycle
      vwc = track("vwc", soil);
      batt = battery ? track("batt", battery) : nullptr; // Sensor_Battery reports millivolts
    }
  protected:
    String body() override {
      const float volts = batt ? (batt->floatValue() / 1000.0) : 0;
      String b = F("id=" CONTROL_HTTPPOST_NODE_ID);
      b += F("&vwc=");  b += String(vwc->floatValue(), 1);
      b += F("&batt="); b += String(volts, 2);
      b += F("&soc=");  b += stateOfCharge(volts);
      b += F("&dist="); b += String(distanceMetres(frugal_iot.wifi->RSSI()), 1);
      return b;
    }
};

void setup() {
  // Battery sensor has to come before pre_setup, all others should come after
  #ifdef SENSOR_BATTERY_PIN
    frugal_iot.configure_battery(SENSOR_BATTERY_PIN, SENSOR_BATTERY_VOLTAGE_DIVIDER);
  #endif
  // Awake for SYSTEM_POWER_WAKE_MS of every SYSTEM_POWER_CYCLE_MS, deep sleep for the rest.
  // Deep sleep between readings - 
  // ESP8266 needs D0 wired to RST, or will never return from deep-sleep, see the note at the top.
  frugal_iot.configure_power(Power_Deep, SYSTEM_POWER_CYCLE_MS, SYSTEM_POWER_WAKE_MS);

  frugal_iot.pre_setup(); // Serial, file system, main config

  frugal_iot.configure_mqtt_enrolled(SYSTEM_MQTT_HOST, SYSTEM_MQTT_ENROL_SECRET);
  frugal_iot.wifi->addWiFi(F(SYSTEM_WIFI_SSID), F(SYSTEM_WIFI_PASSWORD)); // Others can be added on the captive portal

  // 0 % at the dry reading, 100 % at the wet one, the median of several samples, probe powered from its pin
  Sensor_Soil* soil = new Sensor_Soil("soil", "Soil", SENSOR_SOIL_PIN,
    SENSOR_SOIL_DRY, -100.0 / (SENSOR_SOIL_DRY - SENSOR_SOIL_WET), DEFAULT_soil_soil_color, true);
  soil->takeMedian(SENSOR_SOIL_SAMPLES);  // Take the median of multiple readings
  soil->powerPins(SENSOR_SOIL_POWER3v3_PIN, PIN_NONE); // Turn on power to sensor before reading 
  frugal_iot.sensors->add(soil);

  frugal_iot.controls->add(new Control_Hub(soil, frugal_iot.battery)); // Posts to the hub every cycle

  frugal_iot.setup(); // After all the sensors, actuators and controls are added
  Serial.println(F("Kopernik Soil sensor Starting Loop"));
}

void loop() {
  frugal_iot.loop();
}
