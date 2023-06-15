#include <Arduino.h>
#include <avdweb_Switch.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define physical_clock
#define thermostat

const char device[7] = "therm";
const char smart_prefix = 't';
const uint8_t version = 7;

OneWire oneWire(D5);
DallasTemperature sensors(&oneWire);

const int relay_pin = 12;

Switch powerButton = Switch(0);
Switch selectorButton = Switch(2);

bool remote_heating = false;
int smart_heating = -1;
int downtime = 0;
const int default_downtime_plus = 10800;
int downtime_plus = default_downtime_plus;
uint32_t vacation = 0;
bool key_lock = false;

float temperature = -127.0;
bool heating = false;
int heating_time = 0;
float heating_temperature = 0.0;

const float default_minimum_temperature = 7.0;
float minimum_temperature = default_minimum_temperature;
const int default_heating_time_plus = 600;
int heating_time_plus = default_heating_time_plus;
const float default_heating_temperature_plus = 1.0;
float heating_temperature_plus = default_heating_temperature_plus;
const float default_correction = -3.5;
float correction = default_correction;

int selector = 1;
int selector_counter = 0;
String text1;
String text2;
String text3;

static const uint8_t image_data_manual[24] = {
    0x00, 0x00,
    0x06, 0x00,
    0x16, 0x80,
    0x26, 0x40,
    0x46, 0x20,
    0x46, 0x20,
    0x40, 0x20,
    0x40, 0x20,
    0x20, 0x40,
    0x30, 0xc0,
    0x0f, 0x00,
    0x00, 0x00
};

static const uint8_t image_data_remote[24] = {
    0x00, 0x00,
    0x00, 0x00,
    0x0f, 0x00,
    0x11, 0x80,
    0x70, 0x80,
    0xc0, 0x60,
    0x80, 0x10,
    0x80, 0x10,
    0xc0, 0x10,
    0x7f, 0xe0,
    0x00, 0x00,
    0x00, 0x00
};

static const uint8_t image_data_auto[24] = {
    0x00, 0x00,
    0x00, 0x80,
    0x03, 0xe0,
    0x01, 0x40,
    0x03, 0xe0,
    0x18, 0x80,
    0x7e, 0x00,
    0x24, 0x00,
    0x66, 0x00,
    0x7e, 0x00,
    0x18, 0x00,
    0x00, 0x00
};

static const uint8_t image_data_reheating[24] = {
    0x1f, 0x00,
    0x20, 0x80,
    0x44, 0x40,
    0x84, 0x20,
    0x84, 0x20,
    0x84, 0x20,
    0x86, 0x20,
    0x83, 0x20,
    0x40, 0x40,
    0x20, 0x80,
    0x1f, 0x00,
    0x00, 0x00
};
static const uint8_t image_data_downtime[24] = {
    0x00, 0x00,
    0x00, 0x00,
    0x19, 0x80,
    0x19, 0x80,
    0x19, 0x80,
    0x19, 0x80,
    0x19, 0x80,
    0x19, 0x80,
    0x19, 0x80,
    0x19, 0x80,
    0x00, 0x00,
    0x00, 0x00
};

void refreshTheDisplay();
void displayText(String text, int line);
void displayText(String text, int line, bool append);
bool readSettings(bool backup);
void saveSettings();
void saveSettings(bool log);
bool resume();
void saveTheState();
String getValue();
int getHeatingTime();
String getThermostatDetail();
void sayHelloToTheServer();
void uploadEverythingToServer();
void startServices();
void handshake();
void requestForState();
void exchangeOfBasicData();
void powerButtonSingle(void* b);
void powerButtonLong(void* b);
void selectorButtonSingle(void* b);
void readData(const String& payload, bool per_wifi);
void automation();
void smartAction();
void automaticHeatingOff();
void setHeating(bool set, String orderer);
