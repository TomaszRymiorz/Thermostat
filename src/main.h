#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <avdweb_Switch.h>
#include <stdio.h>
#include <time.h>

const char device[7] = "therm";
const int version = 4;

OneWire oneWire(D5);
DallasTemperature sensors(&oneWire);

Switch powerButton = Switch(D3);

const int relay_pin = D6;

struct Smart {
  bool enabled;
  String days;
  float temp;
  int start_time;
  int end_time;
};

bool remote_heating = false;
int smart_heating = -1;
int downtime = 0;
int downtime_plus = 10800;
uint32_t vacation = 0;

float temperature = -127.0;
bool heating = false;
int heating_time = 0;
float heating_temperature = 0.0;

float minimum_temperature = 7.0;
int heating_time_plus = 600;
float heating_temperature_plus = 1.0;
float correction = -3.5;

bool readSettings(bool backup);
void saveSettings();
bool resume();
void saveTheState();
void sayHelloToTheServer();
void introductionToServer();
void startServices();
String getThermostatDetail();
int getHeatingTime();
void handshake();
void requestForState();
void exchangeOfBasicData();
void deleteDeviceMemory();
bool hasTheTemperatureChanged();
void powerButtonSingle(void* s);
void powerButtonLong(void* s);
void automaticHeatingOff();
void readData(String payload, bool per_wifi);
void setSmart();
bool automaticSettings();
bool automaticSettings(bool temperature_changed);
void setHeating(bool set, String note_text);
