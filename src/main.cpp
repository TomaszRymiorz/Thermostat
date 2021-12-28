#include <c_online.h>

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  LittleFS.begin();
  Wire.begin();

  keep_log = LittleFS.exists("/log.txt");

  #ifdef RTC_DS1307
    rtc.begin();
  #endif

  note("iDom Thermostat " + String(version) + "." + String(core_version));
  offline = !LittleFS.exists("/online.txt");
  Serial.print(offline ? " OFFLINE" : " ONLINE");

  sprintf(host_name, "therm_%s", String(WiFi.macAddress()).c_str());
  WiFi.hostname(host_name);

  pinMode(relay_pin, OUTPUT);
  digitalWrite(relay_pin, LOW);

  if (!readSettings(0)) {
    readSettings(1);
  }

  if (RTCisrunning()) {
    start_time = rtc.now().unixtime() - offset - (dst ? 3600 : 0);
  }

  sensors.begin();
  sensors.requestTemperatures();
  temperature = sensors.getTempCByIndex(0) + correction;
  if (!resume()) {
    automaticSettings(true);
  };

  powerButton.setSingleClickCallback(&powerButtonSingle, (void*)"");
  powerButton.setLongPressCallback(&powerButtonLong, (void*)"");

  setupOTA();

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

  DynamicJsonDocument json_object(1024);
  deserializeJson(json_object, file.readString());

  if (json_object.isNull() || json_object.size() < 5) {
    note(String(backup ? "Backup" : "Settings") + " file error");
    file.close();
    return false;
  }

  file.seek(0);
  note("Reading the " + String(backup ? "backup" : "settings") + " file:\n " + file.readString());
  file.close();

  if (json_object.containsKey("ssid")) {
    ssid = json_object["ssid"].as<String>();
  }
  if (json_object.containsKey("password")) {
    password = json_object["password"].as<String>();
  }

  if (json_object.containsKey("smart")) {
    smart_string = json_object["smart"].as<String>();
    setSmart();
  }
  if (json_object.containsKey("uprisings")) {
    uprisings = json_object["uprisings"].as<int>() + 1;
  }
  if (json_object.containsKey("offset")) {
    offset = json_object["offset"].as<int>();
  }
  if (json_object.containsKey("dst")) {
    dst = json_object["dst"].as<bool>();
  }
  if (json_object.containsKey("correction")) {
    correction = json_object["correction"].as<float>();
  }

  if (json_object.containsKey("minimum")) {
    minimum_temperature = json_object["minimum"].as<float>();
  }

  if (json_object.containsKey("plustemp")) {
    heating_temperature_plus = json_object["plustemp"].as<float>();
  }

  if (json_object.containsKey("plustime")) {
    heating_time_plus = json_object["plustime"].as<int>();
  }

  if (json_object.containsKey("downtime")) {
    downtime_plus = json_object["downtime"].as<int>();
  }

  if (json_object.containsKey("vacation")) {
    vacation = json_object["vacation"].as<uint32_t>();
  }

  saveSettings(false);

  return true;
}

void saveSettings() {
  saveSettings(true);
}

void saveSettings(bool log) {
  DynamicJsonDocument json_object(1024);

  json_object["ssid"] = ssid;
  json_object["password"] = password;

  if (smart_string.length() > 2) {
    json_object["smart"] = smart_string;
  }
  json_object["uprisings"] = uprisings;
  if (offset > 0) {
    json_object["offset"] = offset;
  }
  if (dst) {
    json_object["dst"] = dst;
  }
  if (correction != -3.5) {
    json_object["correction"] = correction;
  }
  if (heating_temperature_plus != 1.0) {
    json_object["plustemp"] = heating_temperature_plus;
  }
  if (heating_time_plus != 600) {
    json_object["plustime"] = heating_time_plus;
  }
  if (minimum_temperature != 7) {
    json_object["minimum"] = minimum_temperature;
  }
  if (downtime_plus != 10800) {
    json_object["downtime"] = downtime_plus;
  }
  if (vacation > 0) {
    json_object["vacation"] = vacation;
  }

  if (writeObjectToFile("settings", json_object)) {
    if (log) {
      String logs;
      serializeJson(json_object, logs);
      note("Saving settings:\n " + logs);
    }

    writeObjectToFile("backup", json_object);
  } else {
    note("Saving the settings failed!");
  }
}

bool resume() {
  File file = LittleFS.open("/resume.txt", "r");
  if (!file) {
    return false;
  }

  DynamicJsonDocument json_object(1024);
  deserializeJson(json_object, file.readString());
  file.close();

  if (json_object.isNull() || json_object.size() < 1) {
    return false;
  }

  if (json_object.containsKey("htemp")) {
    heating_temperature = json_object["htemp"].as<float>();
  }
  if (json_object.containsKey("htime")) {
    heating_time = json_object["htime"].as<int>();
    if (getHeatingTime() > 4000 && !RTCisrunning()) {
      heating_time = 0;
    }
  }

  if (heating_temperature > 0 || heating_time > 0) {
    setHeating(true, "resume");
  } else {
    if (LittleFS.exists("/resume.txt")) {
      LittleFS.remove("/resume.txt");
    }
    return false;
  }

  return true;
}

void saveTheState() {
  DynamicJsonDocument json_object(1024);

  if (heating_temperature > 0.0) {
    json_object["htemp"] = heating_temperature;
  }
  if (heating_time > 0) {
    json_object["htime"] = heating_time;
  }

  writeObjectToFile("resume", json_object);
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
  server.on("/log", HTTP_GET, requestForLogs);
  server.on("/log", HTTP_DELETE, clearTheLog);
  server.on("/admin/update", HTTP_POST, manualUpdate);
  server.on("/admin/log", HTTP_POST, activationTheLog);
  server.on("/admin/log", HTTP_DELETE, deactivationTheLog);
  server.begin();

  note(String(host_name) + (MDNS.begin(host_name) ? " started" : " unsuccessful!"));

  MDNS.addService("idom", "tcp", 8080);

  getTime();
  getOfflineData();
}

String getThermostatDetail() {
  return String(RTCisrunning()) + "," + String(start_time) + "," + uprisings + "," + version + "," + correction + "," + minimum_temperature  + "," + heating_temperature_plus + "," + heating_time_plus + "," + downtime_plus + "," + vacation;
}

String getValue() {
  return String(heating);
}

int getHeatingTime() {
  return heating_time > 0 ? (RTCisrunning() ? (heating_time - rtc.now().unixtime()) : heating_time) : 0;
}

void handshake() {
  if (server.hasArg("plain")) {
    readData(server.arg("plain"), true);
  }

  String reply = "\"id\":\"" + WiFi.macAddress()
  + "\",\"value\":" + getValue()
  + ",\"htemp\":" + heating_temperature
  + ",\"htime\":" + String(getHeatingTime())
  + ",\"temp\":" + temperature
  + ",\"correction\":" + correction
  + ",\"minimum\":" + minimum_temperature
  + ",\"plustemp\":" + heating_temperature_plus
  + ",\"plustime\":" + heating_time_plus
  + ",\"downtime\":" + downtime_plus
  + ",\"vacation\":" + vacation
  + ",\"version\":" + version
  + ",\"smart\":\"" + smart_string
  + "\",\"rtc\":" + RTCisrunning()
  + ",\"dst\":" + dst
  + ",\"offset\":" + offset
  + ",\"time\":" + (RTCisrunning() ? String(rtc.now().unixtime() - offset - (dst ? 3600 : 0)) : "0")
  + ",\"active\":" + (RTCisrunning() ? String((rtc.now().unixtime() - offset - (dst ? 3600 : 0)) - start_time) : "0")
  + ",\"uprisings\":" + uprisings
  + ",\"offline\":" + offline;

  Serial.print("\nHandshake");
  server.send(200, "text/plain", "{" + reply + "}");
}

void requestForState() {
  String reply = "\"state\":" + getValue()
  + (heating_temperature > 0 ? ",\"htemp\":" + String(heating_temperature) : "")
  + (heating_time > 0 ? ",\"htime\":" + String(getHeatingTime()) : "")
  + ",\"temp\":" + String(temperature);

  server.send(200, "text/plain", "{" + reply + "}");
}

void exchangeOfBasicData() {
  if (server.hasArg("plain")) {
    readData(server.arg("plain"), true);
  }

  String reply = "\"offset\":" + String(offset) + ",\"dst\":" + String(dst);

  if (RTCisrunning()) {
    reply += ",\"time\":" + String(rtc.now().unixtime() - offset - (dst ? 3600 : 0));
  }

  reply += temperature > -127.0 ? (String(reply.length() > 0 ? "," : "") + "\"temp\":\"" + String(temperature) + "\"") : "";

  server.send(200, "text/plain", "{" + reply + "}");
}


void powerButtonSingle(void* s) {
  if (heating) {
    heating_time = 0;
    heating_temperature = 0.0;
    if (smart_heating > -1) {
      downtime = downtime_plus;
    }
  } else {
    heating_time = RTCisrunning() ? (rtc.now().unixtime() + heating_time_plus) : heating_time_plus;
    heating_temperature = 0.0;
    downtime = 0;
  }
  smart_heating = -1;
  remote_heating = false;
  setHeating(!heating, "manual");
}

void powerButtonLong(void* s) {

  if (heating) {
    heating_time = 0;
    heating_temperature = 0.0;
    if (smart_heating > -1) {
      downtime = RTCisrunning() ? (86400 - (rtc.now().hour() * 3600) - rtc.now().minute() * 60) : 86400;
    }
  } else {
    heating_time = 0;
    heating_temperature = temperature + heating_temperature_plus;
    downtime = 0;
  }
  smart_heating = -1;
  remote_heating = false;
  setHeating(!heating, "manual");
}


void loop() {
  if (WiFi.status() == WL_CONNECTED) {

    ArduinoOTA.handle();
    server.handleClient();
    MDNS.update();
  } else {
    if (!sending_error) {
      note("Wi-Fi connection lost");
    }
    sending_error = true;
  }

  powerButton.poll();

  if (hasTimeChanged()) {
    if (downtime > 0) {
      downtime--;
    }

    if (heating) {
      if (heating_time > 0) {
        saveTheState();
        int time = RTCisrunning() ? (heating_time - rtc.now().unixtime()) : heating_time--;
        if (time <= 0) {
          automaticHeatingOff();
        } else {
          putOnlineData("returned=" + String(temperature) + (heating_time > 0 ?  "c" + String(getHeatingTime()) : ""), false, true);
        }
      }
      if (heating_temperature > 0 && heating_temperature <= temperature) {
        automaticHeatingOff();
      }
    }

    if (!automaticSettings() && loop_time % 2 == 0) {
      getOnlineData();
    };
  }
}

void automaticHeatingOff() {
  heating_time = 0;
  heating_temperature = 0.0;
  smart_heating = -1;
  remote_heating = false;
  setHeating(false, "automatic");
}

bool hasTheTemperatureChanged() {
  if (loop_time % 60 != 0) {
    return false;
  }

  sensors.requestTemperatures();
  float new_temperature = sensors.getTempCByIndex(0) + correction;

  if (temperature != new_temperature) {
    temperature = new_temperature;
    putMultiOfflineData("{\"temp\":" + String(temperature) + "}");
    putOnlineData("returned=" + String(temperature) + (heating_temperature > 0 ? "t" + String(heating_temperature) : "") + (heating_time > 0 ? "c" + String(getHeatingTime()) : ""), false, true);
    return true;
  }

  return false;
}

void readData(String payload, bool per_wifi) {
  DynamicJsonDocument json_object(1024);
  deserializeJson(json_object, payload);

  if (json_object.isNull()) {
    if (payload.length() > 0) {
      note("Parsing failed!");
    }
    return;
  }

  bool settings_change = false;
  bool details_change = false;
  String result = "";

  if (json_object.containsKey("offset")) {
    if (offset != json_object["offset"].as<int>()) {
      if (RTCisrunning() && !json_object.containsKey("time")) {
        rtc.adjust(DateTime((rtc.now().unixtime() - offset) + json_object["offset"].as<int>()));
        note("Time zone change");
      }

      offset = json_object["offset"].as<int>();
      settings_change = true;
    }
  }

  if (json_object.containsKey("dst")) {
    if (dst != strContains(json_object["dst"].as<String>(), "1")) {
      dst = !dst;
      settings_change = true;

      if (RTCisrunning() && !json_object.containsKey("time")) {
        rtc.adjust(DateTime(rtc.now().unixtime() + (dst ? 3600 : -3600)));
        note(dst ? "Summer time" : "Winter time");
      }
    }
  }

  if (json_object.containsKey("time")) {
    int new_time = json_object["time"].as<uint32_t>() + offset + (dst ? 3600 : 0);
    if (new_time > 1546304461) {
      if (RTCisrunning()) {
        if (abs(new_time - (int)rtc.now().unixtime()) > 60) {
          rtc.adjust(DateTime(new_time));
          note("Adjust time");
        }
      } else {
        rtc.adjust(DateTime(new_time));
        note("RTC begin");
        start_time = rtc.now().unixtime() - offset - (dst ? 3600 : 0);
        if (RTCisrunning()) {
          details_change = true;
        }
      }
    }
  }

  if (json_object.containsKey("smart")) {
    if (smart_string != json_object["smart"].as<String>()) {
      smart_string = json_object["smart"].as<String>();
      setSmart();
      if (smart_heating > -1) {
        heating_temperature = 0.0;
        smart_heating = -1;
        setHeating(false, "remote");
      }
      if (per_wifi) {
        result += String(result.length() > 0 ? "&" : "") + "smart=" + getSmartString();
      }
      settings_change = true;
    }
  }

  if (json_object.containsKey("val") && !json_object.containsKey("blinds")) {
    String newValue = json_object["val"].as<String>();
    if (strContains(newValue, "t")) {
      heating_time = 0;
      heating_temperature = newValue.substring(newValue.indexOf("t") + 1).toFloat();
      downtime = 0;
      smart_heating = -1;
      remote_heating = true;
      setHeating(true, "remote");
    }
    if (strContains(newValue, "c")) {
      heating_time = RTCisrunning() ? (rtc.now().unixtime() + newValue.substring(newValue.indexOf("c") + 1).toInt()) : newValue.substring(newValue.indexOf("c") + 1).toInt();
      heating_temperature = 0.0;
      downtime = 0;
      smart_heating = -1;
      remote_heating = true;
      setHeating(true, "remote");
    }
    if (heating && strContains(newValue.substring(0, 1), "0") && !strContains(newValue, "t") && !strContains(newValue, "c") && !strContains(newValue, "v")) {
      heating_time = 0;
      heating_temperature = 0.0;
      if (smart_heating > -1) {
        downtime = downtime_plus;
      }
      smart_heating = -1;
      remote_heating = false;
      setHeating(false, "remote");
    }
    if (strContains(newValue, "v")) {
      vacation = newValue.substring(newValue.indexOf("v") + 1).toInt() + offset + (dst ? 3600 : 0);
      if (vacation > 0 && smart_heating > -1) {
        heating_time = 0;
        heating_temperature = 0.0;
        downtime = 0;
        smart_heating = -1;
        remote_heating = false;
        setHeating(false, "vacation");
      } else {
        putOnlineData("val=" + String(heating));
      }
      per_wifi = true;
      details_change = true;
    }
  }

  if (json_object.containsKey("minimum")) {
    if (minimum_temperature != json_object["minimum"].as<float>()) {
      minimum_temperature = json_object["minimum"].as<float>();
      details_change = true;
    }
  }

  if (json_object.containsKey("correction")) {
    if (correction != json_object["correction"].as<float>()) {
      temperature = (temperature - correction) + json_object["correction"].as<float>();
      correction = json_object["correction"].as<float>();
      details_change = true;
    }
  }

  if (json_object.containsKey("plustemp")) {
    if (heating_temperature_plus != json_object["plustemp"].as<float>()) {
      heating_temperature_plus = json_object["plustemp"].as<float>();
      details_change = true;
    }
  }

  if (json_object.containsKey("plustime")) {
    if (heating_time_plus != json_object["plustime"].as<int>()) {
      heating_time_plus = json_object["plustime"].as<int>();
      details_change = true;
    }
  }

  if (json_object.containsKey("downtime")) {
    if (downtime != json_object["downtime"].as<int>()) {
      downtime_plus = json_object["downtime"].as<int>();
      details_change = true;
    }
  }

  if (settings_change || details_change) {
    note("Received the data:\n " + payload);
    saveSettings();
  }
  if (!offline && (result.length() > 0 || details_change)) {
    if (details_change) {
      result += String(result.length() > 0 ? "&" : "") + "detail=" + getThermostatDetail();
    }
    putOnlineData(result, true);
  }
}

void setSmart() {
  if (smart_string.length() < 2) {
    smart_count = 0;
    return;
  }

  int count = 1;
  smart_count = 1;
  for (char b: smart_string) {
    if (b == ',') {
      count++;
    }
    if (b == smart_prefix) {
      smart_count++;
    }
  }

  if (smart_array != 0) {
    delete [] smart_array;
  }
  smart_array = new Smart[smart_count];
  smart_count = 0;

  String single_smart_string;

  for (int i = 0; i < count; i++) {
    single_smart_string = get1(smart_string, i);
    if (strContains(single_smart_string, String(smart_prefix))) {

      if (strContains(single_smart_string, "/")) {
        smart_array[smart_count].enabled = false;
        single_smart_string = single_smart_string.substring(1);
      } else {
        smart_array[smart_count].enabled = true;
      }

      if (strContains(single_smart_string, "_")) {
        smart_array[smart_count].start_time = single_smart_string.substring(0, single_smart_string.indexOf("_")).toInt();
        single_smart_string = single_smart_string.substring(single_smart_string.indexOf("_") + 1);
      } else {
        smart_array[smart_count].start_time = -1;
      }

      if (strContains(single_smart_string, "-")) {
        smart_array[smart_count].end_time = single_smart_string.substring(single_smart_string.indexOf("-") + 1).toInt();
        single_smart_string = single_smart_string.substring(0, single_smart_string.indexOf("-"));
      } else {
        smart_array[smart_count].end_time = -1;
      }

      if (strContains(single_smart_string, "w")) {
        smart_array[smart_count].days = "w";
      } else {
        smart_array[smart_count].days = strContains(single_smart_string, "o") ? "o" : "";
        smart_array[smart_count].days += strContains(single_smart_string, "u") ? "u" : "";
        smart_array[smart_count].days += strContains(single_smart_string, "e") ? "e" : "";
        smart_array[smart_count].days += strContains(single_smart_string, "h") ? "h" : "";
        smart_array[smart_count].days += strContains(single_smart_string, "r") ? "r" : "";
        smart_array[smart_count].days += strContains(single_smart_string, "a") ? "a" : "";
        smart_array[smart_count].days += strContains(single_smart_string, "s") ? "s" : "";
      }

      smart_array[smart_count].temp = isDigit(single_smart_string.charAt(0)) && isDigit(single_smart_string.charAt(1)) && isDigit(single_smart_string.charAt(3)) ? single_smart_string.substring(0, 4).toFloat() : -1;

      smart_count++;
    }
  }
  note("Smart contains " + String(smart_count) + " of " + String(smart_prefix));
}

bool automaticSettings() {
  return automaticSettings(hasTheTemperatureChanged());
}

bool automaticSettings(bool temperature_changed) {
  bool result = false;
  bool new_heating = false;
  float new_heating_temperature = 0.0;
  DateTime now = rtc.now();
  String log = "Smart ";
  int current_time = -1;

  if (RTCisrunning()) {
    current_time = (now.hour() * 60) + now.minute();
    if (current_time == 120 || current_time == 180) {
      if (now.month() == 3 && now.day() > 24 && days_of_the_week[now.dayOfTheWeek()][0] == 's' && current_time == 120 && !dst) {
        int new_time = now.unixtime() + 3600;
        rtc.adjust(DateTime(new_time));
        dst = true;
        note("Smart set to summer time");
        saveSettings();
      }
      if (now.month() == 10 && now.day() > 24 && days_of_the_week[now.dayOfTheWeek()][0] == 's' && current_time == 180 && dst) {
        int new_time = now.unixtime() - 3600;
        rtc.adjust(DateTime(new_time));
        dst = false;
        note("Smart set to winter time");
        saveSettings();
      }
    }

    if (current_time == (WiFi.localIP()[3] / 2) && now.second() == 0) {
      checkForUpdate(false);
    }
  }

  if (heating_time == 0) {
    if (temperature < minimum_temperature && heating_temperature < minimum_temperature) {
      result = true;
      new_heating = true;
      new_heating_temperature = minimum_temperature;
      smart_heating = -2;
    }

    if (downtime == 0 && (vacation == 0 || (RTCisrunning() && vacation < now.unixtime()))) {
      int i = -1;
      while (++i < smart_count) {
        if (smart_array[i].enabled) {
          bool inTime = false;

          if (strContains(smart_array[i].days, "w") || (RTCisrunning() && strContains(smart_array[i].days, days_of_the_week[now.dayOfTheWeek()]))) {
            inTime = ((smart_array[i].start_time == -1 || (RTCisrunning() && smart_array[i].start_time <= current_time)) && (smart_array[i].end_time == -1 || (RTCisrunning() && smart_array[i].end_time > current_time)));
          }

          if (inTime) {
            if (temperature < smart_array[i].temp && heating_temperature < smart_array[i].temp) {
              result = true;
              new_heating = true;
              smart_heating = i;
              if (new_heating_temperature < smart_array[i].temp) {
                new_heating_temperature = smart_array[i].temp;
              }
            }
          } else {
            if (RTCisrunning() && smart_heating == i) {
              result = true;
              smart_heating = -1;
            }
          }
        }
      }
    }
  }

  if (result && (heating != new_heating || heating_temperature != new_heating_temperature)) {
    heating_temperature = new_heating_temperature;
    heating_time = 0;
    remote_heating = false;
    setHeating(new_heating, "smart");

    return true;
  }

  return false;
}

void setHeating(bool set, String note_text) {
  if (heating != set) {
    heating = set;
    digitalWrite(relay_pin, set);
  }

  note(note_text + " switch-" + String(set ? "on" : "off"));
  putOnlineData("val=" + String(set) + "&returned=" + String(temperature) + (heating_time > 0 ? "c" + String(getHeatingTime()) : "") + (heating_temperature > 0 ? "t" + String(heating_temperature) : ""));

  if (set) {
    saveTheState();
  } else {
    if (LittleFS.exists("/resume.txt")) {
      LittleFS.remove("/resume.txt");
    }
  }

  delay(500);
}
