#include <c_online.h>

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  LittleFS.begin();
  Wire.begin();

  keepLog = LittleFS.exists("/log.txt");

  note("iDom Thermostat " + String(version));
  Serial.print("\nDevice ID: " + WiFi.macAddress());
  offline = !LittleFS.exists("/online.txt");
  Serial.printf("\nThe device is set to %s mode", offline ? "OFFLINE" : "ONLINE");

  sprintf(hostName, "therm_%s", String(WiFi.macAddress()).c_str());
  WiFi.hostname(hostName);

  pinMode(switch_pin, OUTPUT);
  digitalWrite(switch_pin, LOW);

  if (!readSettings(0)) {
    readSettings(1);
  }

  RTC.begin();
  if (RTC.isrunning()) {
    startTime = RTC.now().unixtime() - offset - (dst ? 3600 : 0);
  }
  Serial.printf("\nRTC initialization %s", startTime != 0 ? "completed" : "failed!");

  sensors.begin();
  sensors.requestTemperatures();
  temperature = sensors.getTempCByIndex(0) + correction;
  if (!resume()) {
    automaticSettings(true);
  };

  powerButton.setSingleClickCallback(&powerButtonSingle, (void*)"");
  powerButton.setLongPressCallback(&powerButtonLong, (void*)"");

  if (ssid != "" && password != "") {
    connectingToWifi();
  } else {
    initiatingWPS();
  }
}


bool readSettings(bool backup) {
  File file = LittleFS.open(backup ? "/backup.txt" : "/settings.txt", "r");
  if (!file) {
    note("The " + String(backup ? "backup" : "settings") + " file cannot be read");
    return false;
  }

  DynamicJsonDocument jsonObject(1024);
  deserializeJson(jsonObject, file.readString());
  file.close();

  if (jsonObject.isNull() || jsonObject.size() < 5) {
    note(String(backup ? "Backup" : "Settings") + " file error");
    return false;
  }

  if (jsonObject.containsKey("ssid")) {
    ssid = jsonObject["ssid"].as<String>();
  }
  if (jsonObject.containsKey("password")) {
    password = jsonObject["password"].as<String>();
  }

  if (jsonObject.containsKey("smart")) {
    smartString = jsonObject["smart"].as<String>();
    setSmart();
  }
  if (jsonObject.containsKey("uprisings")) {
    uprisings = jsonObject["uprisings"].as<int>() + 1;
  }
  if (jsonObject.containsKey("offset")) {
    offset = jsonObject["offset"].as<int>();
  }
  if (jsonObject.containsKey("dst")) {
    dst = jsonObject["dst"].as<bool>();
  }
  if (jsonObject.containsKey("correction")) {
    correction = jsonObject["correction"].as<float>();
  }

  if (jsonObject.containsKey("minimum")) {
    minimumTemperature = jsonObject["minimum"].as<float>();
  }

  if (jsonObject.containsKey("plustemp")) {
    heatingTemperaturePlus = jsonObject["plustemp"].as<float>();
  }

  if (jsonObject.containsKey("plustime")) {
    heatingTimePlus = jsonObject["plustime"].as<int>();
  }

  if (jsonObject.containsKey("downtime")) {
    downtimePlus = jsonObject["downtime"].as<int>();
  }

  if (jsonObject.containsKey("vacation")) {
    vacation = jsonObject["vacation"].as<uint32_t>();
  }

  String logs;
  serializeJson(jsonObject, logs);
  note("Reading the " + String(backup ? "backup" : "settings") + " file:\n " + logs);

  saveSettings();

  return true;
}

void saveSettings() {
  DynamicJsonDocument jsonObject(1024);

  jsonObject["ssid"] = ssid;
  jsonObject["password"] = password;

  jsonObject["smart"] = smartString;
  jsonObject["uprisings"] = uprisings;
  jsonObject["offset"] = offset;
  jsonObject["dst"] = dst;
  jsonObject["correction"] = correction;
  jsonObject["plustemp"] = heatingTemperaturePlus;
  jsonObject["plustime"] = heatingTimePlus;
  jsonObject["minimum"] = minimumTemperature;
  jsonObject["downtime"] = downtimePlus;
  jsonObject["vacation"] = vacation;

  if (writeObjectToFile("settings", jsonObject)) {
    String logs;
    serializeJson(jsonObject, logs);
    note("Saving settings:\n " + logs);

    writeObjectToFile("backup", jsonObject);
  } else {
    note("Saving the settings failed!");
  }
}

bool resume() {
  File file = LittleFS.open("/resume.txt", "r");
  if (!file) {
    return false;
  }

  DynamicJsonDocument jsonObject(1024);
  deserializeJson(jsonObject, file.readString());
  file.close();

  if (!jsonObject.isNull() && jsonObject.size() > 0) {
    if (jsonObject.containsKey("htemp")) {
      heatingTemperature = jsonObject["htemp"].as<float>();
    }
    if (jsonObject.containsKey("htime")) {
      heatingTime = jsonObject["htime"].as<int>();
      if (getHeatingTime() > 4000 && !RTC.isrunning()) {
        heatingTime = 0;
      }
    }

    if (heatingTemperature > 0 || heatingTime > 0) {
      setHeating(true, "resume");
    } else {
      if (LittleFS.exists("/resume.txt")) {
        LittleFS.remove("/resume.txt");
      }
      return false;
    }
  }
  return true;
}

void saveTheState() {
  DynamicJsonDocument jsonObject(1024);

  if (heatingTemperature > 0) {
    jsonObject["htemp"] = heatingTemperature;
  }
  if (heatingTime > 0) {
    jsonObject["htime"] = heatingTime;
  }

  writeObjectToFile("resume", jsonObject);
}


void sayHelloToTheServer() {
  // This function is only available with a ready-made iDom device.
}

void introductionToServer() {
  // This function is only available with a ready-made iDom device.
}

void startServices() {
  server.on("/hello", HTTP_POST, handshake);
  server.on("/set", HTTP_PUT, receivedOfflineData);
  server.on("/state", HTTP_GET, requestForState);
  server.on("/basicdata", HTTP_POST, exchangeOfBasicData);
  server.on("/priority", HTTP_POST, confirmationOfPriority);
  server.on("/log", HTTP_GET, requestForLogs);
  server.on("/log", HTTP_DELETE, clearTheLog);
  server.on("/admin/log", HTTP_POST, activationTheLog);
  server.on("/admin/log", HTTP_DELETE, deactivationTheLog);
  server.on("/admin/wifisettings", HTTP_DELETE, deleteWiFiSettings);
  server.begin();

  note("Launch of services. " + String(hostName) + (MDNS.begin(hostName) ? " started." : " unsuccessful!"));

  MDNS.addService("idom", "tcp", 8080);

  if (!offline) {
    prime = true;
  }
  networkedDevices = WiFi.macAddress();
  getOfflineData(true, true);
}

String getThermostatDetail() {
  return String(RTC.isrunning()) + "," + String(startTime) + "," + uprisings + "," + version + "," + correction + "," + minimumTemperature  + "," + heatingTemperaturePlus + "," + heatingTimePlus + "," + downtimePlus + "," + vacation;
}

int getHeatingTime() {
  return heatingTime > 0 ? (RTC.isrunning() ? (heatingTime - RTC.now().unixtime()) : heatingTime) : 0;
}

void handshake() {
  readData(server.arg("plain"), true);

  String reply = "\"id\":\"" + WiFi.macAddress()
  + "\",\"version\":" + version
  + ",\"value\":" + heating
  + ",\"htemp\":" + heatingTemperature
  + ",\"htime\":" + String(getHeatingTime())
  + ",\"temp\":" + temperature
  + ",\"smart\":\"" + smartString
  + "\",\"correction\":" + correction
  + ",\"minimum\":" + minimumTemperature
  + ",\"rtc\":" + RTC.isrunning()
  + ",\"dst\":" + dst
  + ",\"offset\":" + offset
  + ",\"time\":" + String(RTC.now().unixtime() - offset - (dst ? 3600 : 0))
  + ",\"active\":" + String(startTime != 0 ? RTC.now().unixtime() - offset - (dst ? 3600 : 0) - startTime : 0)
  + ",\"uprisings\":" + uprisings
  + ",\"offline\":" + offline
  + ",\"prime\":" + prime
  + ",\"plustemp\":" + heatingTemperaturePlus
  + ",\"plustime\":" + heatingTimePlus
  + ",\"downtime\":" + downtimePlus
  + ",\"vacation\":" + vacation
  + ",\"devices\":\"" + networkedDevices + "\"";

  Serial.print("\nHandshake");
  server.send(200, "text/plain", "{" + reply + "}");
}

void requestForState() {
  String reply = "\"state\":" + String(heating)
  + (heatingTemperature > 0 ? ",\"htemp\":" + String(heatingTemperature) : "")
  + (heatingTime > 0 ? ",\"htime\":" + String(getHeatingTime()) : "")
  + ",\"temp\":" + String(temperature);

  server.send(200, "text/plain", "{" + reply + "}");
}

void exchangeOfBasicData() {
  readData(server.arg("plain"), true);

  String reply = RTC.isrunning() ? ("\"time\":" + String(RTC.now().unixtime() - offset - (dst ? 3600 : 0))
  + ",\"offset\":" + offset
  + ",\"dst\":" + String(dst)) : "";
  reply += !offline && prime ? (String(reply.length() > 0 ? "," : "") + "\"prime\":" + String(prime)) : "";

  reply += String(reply.length() > 0 ? "," : "") + "\"id\":\"" + String(WiFi.macAddress()) + "\"";

  server.send(200, "text/plain", "{" + reply + "}");
}

void confirmationOfPriority() {
  readData(server.arg("plain"), true);

  String reply = "\"id\":\"" + String(WiFi.macAddress()) + "\"";

  reply += prime ? (",\"prime\":" + String(prime)) : "";

  server.send(200, "text/plain", "{" + reply + "}");
}


void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    if (!sendingError) {
      note("Wi-Fi connection lost");
    }
    sendingError = true;
  }

  server.handleClient();
  MDNS.update();

  powerButton.poll();

  if (hasTimeChanged()) {
    if (downtime > 0) {
      downtime--;
    }

    if (heating) {
      if (heatingTime > 0) {
        saveTheState();
        int time = RTC.isrunning() ? (heatingTime - RTC.now().unixtime()) : heatingTime--;
        if (time <= 0) {
          automaticHeatingOff();
        } else {
          putOnlineData("detail", "returned=" + String(temperature) + (heatingTime > 0 ?  "c" + String(getHeatingTime()) : ""), false);
        }
      }
      if (heatingTemperature > 0 && heatingTemperature <= temperature) {
        automaticHeatingOff();
      }
    }

    if (!automaticSettings() && loopTime % 2 == 0) {
      getOnlineData();
    };
  }
}


void powerButtonSingle(void* s) {
  if (heating) {
    heatingTime = 0;
    heatingTemperature = 0.0;
    if (smartHeating > -1) {
      downtime = downtimePlus;
    }
  } else {
    heatingTime = RTC.isrunning() ? (RTC.now().unixtime() + heatingTimePlus) : heatingTimePlus;
    heatingTemperature = 0.0;
    downtime = 0;
  }
  smartHeating = -1;
  remoteHeating = false;
  setHeating(!heating, "Manual");
}

void powerButtonLong(void* s) {

  if (heating) {
    heatingTime = 0;
    heatingTemperature = 0.0;
    if (smartHeating > -1) {
      downtime = RTC.isrunning() ? (86400 - (RTC.now().hour() * 3600) - RTC.now().minute() * 60) : 86400;
    }
  } else {
    heatingTime = 0;
    heatingTemperature = temperature + heatingTemperaturePlus;
    downtime = 0;
  }
  smartHeating = -1;
  remoteHeating = false;
  setHeating(!heating, "Manual");
}

void automaticHeatingOff() {
  heatingTime = 0;
  heatingTemperature = 0.0;
  smartHeating = -1;
  remoteHeating = false;
  setHeating(false, "Automatic");
}

bool hasTheTemperatureChanged() {
  if (loopTime % 60 != 0) {
    return false;
  }

  sensors.requestTemperatures();
  float newTemperature = sensors.getTempCByIndex(0) + correction;

  if (temperature != newTemperature) {
    temperature = newTemperature;
    putOnlineData("detail", "returned=" + String(temperature) + (heatingTemperature > 0 ? "t" + String(heatingTemperature) : "") + (heatingTime > 0 ? "c" + String(getHeatingTime()) : ""), false);
    return true;
  }

  return false;
}

void readData(String payload, bool perWiFi) {
  DynamicJsonDocument jsonObject(1024);
  deserializeJson(jsonObject, payload);

  if (jsonObject.isNull()) {
    if (payload.length() > 0) {
      Serial.print("\n Parsing failed!");
    }
    return;
  }

  if (jsonObject.containsKey("apk")) {
    perWiFi = jsonObject["apk"].as<bool>();
  }

  if (jsonObject.containsKey("id")) {
    String newNetworkedDevices = jsonObject["id"].as<String>();
    if (!strContains(networkedDevices, newNetworkedDevices)) {
      networkedDevices +=  "," + newNetworkedDevices;
    }
  }

  if (jsonObject.containsKey("prime")) {
    prime = false;
  }

  bool settingsChange = false;
  bool detailsChange = false;
  String result = "";

  uint32_t newTime = 0;
  if (jsonObject.containsKey("offset")) {
    int newOffset = jsonObject["offset"].as<int>();
    if (offset != newOffset) {
      if (RTC.isrunning() && !jsonObject.containsKey("time")) {
        RTC.adjust(DateTime((RTC.now().unixtime() - offset) + newOffset));
        note("Time zone change");
      }

      offset = newOffset;
      settingsChange = true;
    }
  }

  if (jsonObject.containsKey("dst")) {
    bool newDST = jsonObject["dst"].as<bool>();
    if (dst != newDST) {
      if (RTC.isrunning() && !jsonObject.containsKey("time")) {
        if (newDST) {
          newTime = RTC.now().unixtime() + 3600;
        } else {
          newTime = RTC.now().unixtime() - 3600;
        }
        RTC.adjust(DateTime(newTime));
        note(newDST ? "Summer time" : "Winter time");
      }

      dst = newDST;
      settingsChange = true;
    }
  }

  if (jsonObject.containsKey("time")) {
    newTime = jsonObject["time"].as<uint32_t>() + offset + (dst ? 3600 : 0);
    if (newTime > 1546304461) {
      if (RTC.isrunning()) {
        if (abs(newTime - RTC.now().unixtime()) > 60) {
          RTC.adjust(DateTime(newTime));
        }
      } else {
        RTC.adjust(DateTime(newTime));
        note("Adjust time");
        startTime = RTC.now().unixtime() - offset - (dst ? 3600 : 0);
        if (RTC.isrunning()) {
          sayHelloToTheServer();
        }
      }
    }
  }

  if (jsonObject.containsKey("up")) {
    uint32_t newUpdateTime = jsonObject["up"].as<uint32_t>();
    if (updateTime < newUpdateTime) {
      updateTime = newUpdateTime;
    }
  }

  if (jsonObject.containsKey("smart")) {
    String newSmartString = jsonObject["smart"].as<String>();
    if (smartString != newSmartString) {
      smartString = newSmartString;
      setSmart();
      if (smartHeating > -1) {
        heatingTemperature = 0.0;
        smartHeating = -1;
        setHeating(false, "Remote");
      }
      result = "smart=" + newSmartString;
      settingsChange = true;
    }
  }

  if (jsonObject.containsKey("val")) {
    String newHeating = jsonObject["val"].as<String>();
    if (strContains(newHeating, "t")) {
      heatingTime = 0;
      heatingTemperature = newHeating.substring(newHeating.indexOf("t") + 1).toFloat();
      downtime = 0;
      smartHeating = -1;
      remoteHeating = true;
      setHeating(true, "Remote");
    }
    if (strContains(newHeating, "c")) {
      heatingTime = RTC.isrunning() ? (RTC.now().unixtime() + newHeating.substring(newHeating.indexOf("c") + 1).toInt()) : newHeating.substring(newHeating.indexOf("c") + 1).toInt();
      heatingTemperature = 0.0;
      downtime = 0;
      smartHeating = -1;
      remoteHeating = true;
      setHeating(true, "Remote");
    }
    if (heating && strContains(newHeating.substring(0, 1), "0") && !strContains(newHeating, "t") && !strContains(newHeating, "c") && !strContains(newHeating, "v")) {
      heatingTime = 0;
      heatingTemperature = 0.0;
      if (smartHeating > -1) {
        downtime = downtimePlus;
      }
      smartHeating = -1;
      remoteHeating = false;
      setHeating(false, "Remote");
    }
    if (strContains(newHeating, "v")) {
      vacation = newHeating.substring(newHeating.indexOf("v") + 1).toInt() + offset + (dst ? 3600 : 0);
      if (vacation > 0 && smartHeating > -1) {
        heatingTime = 0;
        heatingTemperature = 0.0;
        downtime = 0;
        smartHeating = -1;
        remoteHeating = false;
        setHeating(false, "Vacation");
      } else {
        putOnlineData("detail", "val=" + String(heating));
      }
      perWiFi = true;
      detailsChange = true;
    }
  }

  if (jsonObject.containsKey("minimum")) {
    float newMinimumTemperature = jsonObject["minimum"].as<float>();
    if (minimumTemperature != newMinimumTemperature) {
      minimumTemperature = newMinimumTemperature;
      detailsChange = true;
    }
  }

  if (jsonObject.containsKey("correction")) {
    float newCorrection = jsonObject["correction"].as<float>();
    if (correction != newCorrection) {
      temperature = (temperature - correction) + newCorrection;
      correction = newCorrection;
      detailsChange = true;
    }
  }

  if (jsonObject.containsKey("plustemp")) {
    float newHeatingTemperaturePlus = jsonObject["plustemp"].as<float>();
    if (heatingTemperaturePlus != newHeatingTemperaturePlus) {
      heatingTemperaturePlus = newHeatingTemperaturePlus;
      detailsChange = true;
    }
  }

  if (jsonObject.containsKey("plustime")) {
    int newHeatingTimePlus = jsonObject["plustime"].as<int>();
    if (heatingTimePlus != newHeatingTimePlus) {
      heatingTimePlus = newHeatingTimePlus;
      detailsChange = true;
    }
  }

  if (jsonObject.containsKey("downtime")) {
    int newDowntimePlus = jsonObject["downtime"].as<int>();
    if (downtime != newDowntimePlus) {
      downtimePlus = newDowntimePlus;
      detailsChange = true;
    }
  }

  if (settingsChange || detailsChange) {
    note("Received the data:\n " + payload);
    saveSettings();
  }
  if (perWiFi && (result.length() > 0 || detailsChange)) {
    if (detailsChange) {
      result += String(result.length() > 0 ? "&" : "") + "detail=" + getThermostatDetail();
    }
    putOnlineData("detail", result);
  }
}

void setSmart() {
  if (smartString.length() < 2) {
    smartCount = 0;
    return;
  }

  String smart;
  String days;
  float temp;
  int startTime;
  int endTime;
  bool enabled;

  int count = 1;
  smartCount = 1;
  for (byte b: smartString) {
    if (b == ',') {
      count++;
    }
    if (b == 't') {
      smartCount++;
    }
  }

  if (smartArray != 0) {
    delete [] smartArray;
  }
  smartArray = new Smart[smartCount];
  smartCount = 0;

  for (int i = 0; i < count; i++) {
    smart = get1(smartString, i);
    if (smart.length() > 0 && strContains(smart, "t")) {
      enabled = !strContains(smart, "/");
      smart = enabled ? smart : smart.substring(1);

      startTime = strContains(smart, "_") ? smart.substring(0, smart.indexOf("_")).toInt() : -1;
      endTime = strContains(smart, "-") ? smart.substring(smart.indexOf("-") + 1).toInt() : -1;

      smart = strContains(smart, "_") ? smart.substring(smart.indexOf("_") + 1) : smart;
      smart = strContains(smart, "-") ? smart.substring(0, smart.indexOf("-")) : smart;

      days = strContains(smart, "w") ? "w" : "";
      days += strContains(smart, "o") ? "o" : "";
      days += strContains(smart, "u") ? "u" : "";
      days += strContains(smart, "e") ? "e" : "";
      days += strContains(smart, "h") ? "h" : "";
      days += strContains(smart, "r") ? "r" : "";
      days += strContains(smart, "a") ? "a" : "";
      days += strContains(smart, "s") ? "s" : "";

      temp = isDigit(smart.charAt(0)) && isDigit(smart.charAt(1)) && isDigit(smart.charAt(3)) ? smart.substring(0, 4).toFloat() : -1;

      smartArray[smartCount++] = (Smart) {days, temp, startTime, endTime, enabled};
    }
  }
}

bool automaticSettings() {
  return automaticSettings(hasTheTemperatureChanged());
}

bool automaticSettings(bool temperatureChanged) {
  bool result = false;
  bool newHeating = false;
  float newHeatingTemperature = 0.0;
  DateTime now = RTC.now();

  if (heatingTime == 0) {
    if (temperature < minimumTemperature && heatingTemperature < minimumTemperature) {
      result = true;
      newHeating = true;
      newHeatingTemperature = minimumTemperature;
      smartHeating = -2;
    }

    if (downtime == 0 && (vacation == 0 || (RTC.isrunning() && vacation < now.unixtime()))) {
      int i = -1;
      while (++i < smartCount) {
        if (smartArray[i].enabled) {
          bool inTime = false;

          if (strContains(smartArray[i].days, "w") || (RTC.isrunning() && strContains(smartArray[i].days, daysOfTheWeek[now.dayOfTheWeek()]))) {

            int currentTime = RTC.isrunning() ? ((now.hour() * 60) + now.minute()) : -1;
            inTime = ((smartArray[i].startTime == -1 || (RTC.isrunning() && smartArray[i].startTime <= currentTime)) && (smartArray[i].endTime == -1 || (RTC.isrunning() && smartArray[i].endTime > currentTime)));
          }

          if (inTime) {
            if (temperature < smartArray[i].temp && heatingTemperature < smartArray[i].temp) {
              result = true;
              newHeating = true;
              smartHeating = i;
              if (newHeatingTemperature < smartArray[i].temp) {
                newHeatingTemperature = smartArray[i].temp;
              }
            }
          } else {
            if (RTC.isrunning() && smartHeating == i) {
              result = true;
              smartHeating = -1;
            }
          }
        }
      }
    }
  }

  if (RTC.isrunning()) {
    int currentTime = (now.hour() * 60) + now.minute();

    if (currentTime == 120 || currentTime == 180) {
      if (now.month() == 3 && now.day() > 24 && daysOfTheWeek[now.dayOfTheWeek()][0] == 's' && currentTime == 120 && !dst) {
        int newTime = RTC.now().unixtime() + 3600;
        RTC.adjust(DateTime(newTime));
        dst = true;
        saveSettings();
        note("Smart set to summer time");
      }
      if (now.month() == 10 && now.day() > 24 && daysOfTheWeek[now.dayOfTheWeek()][0] == 's' && currentTime == 180 && dst) {
        int newTime = RTC.now().unixtime() - 3600;
        RTC.adjust(DateTime(newTime));
        dst = false;
        saveSettings();
        note("Smart set to winter time");
      }
    }
  }

  if (result && (heating != newHeating || heatingTemperature != newHeatingTemperature)) {
    heatingTemperature = newHeatingTemperature;
    heatingTime = 0;
    remoteHeating = false;
    setHeating(newHeating, "Smart");

    return true;
  }

  return false;
}

void setHeating(bool set, String noteText) {
  if (heating != set) {
    heating = set;
    digitalWrite(switch_pin, set);
  }

  note(noteText + " switch-" + String(set ? "on" : "off"));
  putOnlineData("detail", "val=" + String(set) + "&returned=" + String(temperature) + (heatingTime > 0 ? "c" + String(getHeatingTime()) : "") + (heatingTemperature > 0 ? "t" + String(heatingTemperature) : ""));

  if (set) {
    saveTheState();
  } else {
    if (LittleFS.exists("/resume.txt")) {
      LittleFS.remove("/resume.txt");
    }
  }

  delay(500);
}
