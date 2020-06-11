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

const int switch_pin = D6;

struct Smart {
  String days;
  float temp;
  int startTime;
  int endTime;
  bool enabled;
};

bool remoteHeating = false;
int smartHeating = -1;
int downtime = 0;
int downtimePlus = 10800;
uint32_t vacation = 0;

float temperature = -127.0;
bool heating = false;
int heatingTime = 0;
float heatingTemperature = 0.0;

float minimumTemperature = 7.0;
int heatingTimePlus = 600;
float heatingTemperaturePlus = 1.0;
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
void confirmationOfPriority();
void deleteDeviceMemory();
bool hasTheTemperatureChanged();
void powerButtonSingle(void* s);
void powerButtonLong(void* s);
void automaticHeatingOff();
void readData(String payload, bool perWiFi);
void setSmart();
bool automaticSettings();
bool automaticSettings(bool temperatureChanged);
void setHeating(bool set, String noteText);
