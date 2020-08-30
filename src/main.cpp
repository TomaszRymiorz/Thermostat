#include <c_online.h>

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  LittleFS.begin();
  Wire.begin();

  keep_log = LittleFS.exists("/log.txt");

  note("iDom Thermostat " + String(version));
  Serial.print("\nDevice ID: " + WiFi.macAddress());
  offline = !LittleFS.exists("/online.txt");
  Serial.printf("\nThe device is set to %s mode", offline ? "OFFLINE" : "ONLINE");

  sprintf(host_name, "therm_%s", String(WiFi.macAddress()).c_str());
  WiFi.hostname(host_name);

  pinMode(relay_pin, OUTPUT);
  digitalWrite(relay_pin, LOW);

  if (!readSettings(0)) {
    readSettings(1);
  }

  RTC.begin();
  if (RTC.isrunning()) {
    start_time = RTC.now().unixtime() - offset - (dst ? 3600 : 0);
  }
  Serial.printf("\nRTC initialization %s", start_time != 0 ? "completed" : "failed!");

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

  DynamicJsonDocument json_object(1024);
  deserializeJson(json_object, file.readString());
  file.close();

  if (json_object.isNull() || json_object.size() < 5) {
    note(String(backup ? "Backup" : "Settings") + " file error");
    return false;
  }

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

  String logs;
  serializeJson(json_object, logs);
  note("Reading the " + String(backup ? "backup" : "settings") + " file:\n " + logs);

  saveSettings();

  return true;
}

void saveSettings() {
  DynamicJsonDocument json_object(1024);

  json_object["ssid"] = ssid;
  json_object["password"] = password;

  json_object["smart"] = smart_string;
  json_object["uprisings"] = uprisings;
  json_object["offset"] = offset;
  json_object["dst"] = dst;
  json_object["correction"] = correction;
  json_object["plustemp"] = heating_temperature_plus;
  json_object["plustime"] = heating_time_plus;
  json_object["minimum"] = minimum_temperature;
  json_object["downtime"] = downtime_plus;
  json_object["vacation"] = vacation;

  if (writeObjectToFile("settings", json_object)) {
    String logs;
    serializeJson(json_object, logs);
    note("Saving settings:\n " + logs);

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

  if (!json_object.isNull() && json_object.size() > 0) {
    if (json_object.containsKey("htemp")) {
      heating_temperature = json_object["htemp"].as<float>();
    }
    if (json_object.containsKey("htime")) {
      heating_time = json_object["htime"].as<int>();
      if (getHeatingTime() > 4000 && !RTC.isrunning()) {
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
  }
  return true;
}

void saveTheState() {
  DynamicJsonDocument json_object(1024);

  if (heating_temperature > 0) {
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
  server.on("/priority", HTTP_POST, confirmationOfPriority);
  server.on("/log", HTTP_GET, requestForLogs);
  server.on("/log", HTTP_DELETE, clearTheLog);
  server.on("/admin/log", HTTP_POST, activationTheLog);
  server.on("/admin/log", HTTP_DELETE, deactivationTheLog);
  server.on("/admin/wifisettings", HTTP_DELETE, deleteWiFiSettings);
  server.begin();

  note("Launch of services. " + String(host_name) + (MDNS.begin(host_name) ? " started." : " unsuccessful!"));

  MDNS.addService("idom", "tcp", 8080);

  if (!offline) {
    prime = true;
  }
  networked_devices = WiFi.macAddress();
  getOfflineData(true, true);
}

String getThermostatDetail() {
  return String(RTC.isrunning()) + "," + String(start_time) + "," + uprisings + "," + version + "," + correction + "," + minimum_temperature  + "," + heating_temperature_plus + "," + heating_time_plus + "," + downtime_plus + "," + vacation;
}

int getHeatingTime() {
  return heating_time > 0 ? (RTC.isrunning() ? (heating_time - RTC.now().unixtime()) : heating_time) : 0;
}

void handshake() {
  readData(server.arg("plain"), true);

  String reply = "\"id\":\"" + WiFi.macAddress()
  + "\",\"value\":" + heating
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
  + "\",\"rtc\":" + RTC.isrunning()
  + ",\"dst\":" + dst
  + ",\"offset\":" + offset
  + ",\"time\":" + String(RTC.now().unixtime() - offset - (dst ? 3600 : 0))
  + ",\"active\":" + String(start_time != 0 ? RTC.now().unixtime() - offset - (dst ? 3600 : 0) - start_time : 0)
  + ",\"uprisings\":" + uprisings
  + ",\"offline\":" + offline
  + ",\"prime\":" + prime
  + ",\"devices\":\"" + networked_devices + "\"";

  Serial.print("\nHandshake");
  server.send(200, "text/plain", "{" + reply + "}");
}

void requestForState() {
  String reply = "\"state\":" + String(heating)
  + (heating_temperature > 0 ? ",\"htemp\":" + String(heating_temperature) : "")
  + (heating_time > 0 ? ",\"htime\":" + String(getHeatingTime()) : "")
  + ",\"temp\":" + String(temperature);

  server.send(200, "text/plain", "{" + reply + "}");
}

void exchangeOfBasicData() {
  readData(server.arg("plain"), true);

  String reply = RTC.isrunning() ? ("\"time\":" + String(RTC.now().unixtime() - offset - (dst ? 3600 : 0))
  + ",\"offset\":" + offset
  + ",\"dst\":" + String(dst)) : "";

  reply += temperature > -127.0 ? (String(reply.length() > 0 ? "," : "") + "\"temp\":\"" + String(temperature) + "\"") : "";

  reply += !offline && prime ? (String(reply.length() > 0 ? "," : "") + "\"prime\":" + String(prime)) : "";

  reply += String(reply.length() > 0 ? "," : "") + "\"id\":\"" + String(WiFi.macAddress()) + "\"";

  server.send(200, "text/plain", "{" + reply + "}");
}


void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    if (!sending_error) {
      note("Wi-Fi connection lost");
    }
    sending_error = true;
  }

  server.handleClient();
  MDNS.update();

  powerButton.poll();

  if (hasTimeChanged()) {
    if (downtime > 0) {
      downtime--;
    }

    if (heating) {
      if (heating_time > 0) {
        saveTheState();
        int time = RTC.isrunning() ? (heating_time - RTC.now().unixtime()) : heating_time--;
        if (time <= 0) {
          automaticHeatingOff();
        } else {
          putOnlineData("detail", "returned=" + String(temperature) + (heating_time > 0 ?  "c" + String(getHeatingTime()) : ""), false);
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


void powerButtonSingle(void* s) {
  if (heating) {
    heating_time = 0;
    heating_temperature = 0.0;
    if (smart_heating > -1) {
      downtime = downtime_plus;
    }
  } else {
    heating_time = RTC.isrunning() ? (RTC.now().unixtime() + heating_time_plus) : heating_time_plus;
    heating_temperature = 0.0;
    downtime = 0;
  }
  smart_heating = -1;
  remote_heating = false;
  setHeating(!heating, "Manual");
}

void powerButtonLong(void* s) {

  if (heating) {
    heating_time = 0;
    heating_temperature = 0.0;
    if (smart_heating > -1) {
      downtime = RTC.isrunning() ? (86400 - (RTC.now().hour() * 3600) - RTC.now().minute() * 60) : 86400;
    }
  } else {
    heating_time = 0;
    heating_temperature = temperature + heating_temperature_plus;
    downtime = 0;
  }
  smart_heating = -1;
  remote_heating = false;
  setHeating(!heating, "Manual");
}

void automaticHeatingOff() {
  heating_time = 0;
  heating_temperature = 0.0;
  smart_heating = -1;
  remote_heating = false;
  setHeating(false, "Automatic");
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
    putOnlineData("detail", "returned=" + String(temperature) + (heating_temperature > 0 ? "t" + String(heating_temperature) : "") + (heating_time > 0 ? "c" + String(getHeatingTime()) : ""), false);
    return true;
  }

  return false;
}

void readData(String payload, bool per_wifi) {
  DynamicJsonDocument json_object(1024);
  deserializeJson(json_object, payload);

  if (json_object.isNull()) {
    if (payload.length() > 0) {
      Serial.print("\n Parsing failed!");
    }
    return;
  }

  if (json_object.containsKey("apk")) {
    per_wifi = json_object["apk"].as<bool>();
  }

  if (json_object.containsKey("id")) {
    String new_networked_devices = json_object["id"].as<String>();
    if (!strContains(networked_devices, new_networked_devices)) {
      networked_devices +=  "," + new_networked_devices;
    }
  }

  if (json_object.containsKey("prime")) {
    prime = false;
  }

  bool settings_change = false;
  bool details_change = false;
  String result = "";

  uint32_t new_time = 0;
  if (json_object.containsKey("offset")) {
    int new_offset = json_object["offset"].as<int>();
    if (offset != new_offset) {
      if (RTC.isrunning() && !json_object.containsKey("time")) {
        RTC.adjust(DateTime((RTC.now().unixtime() - offset) + new_offset));
        note("Time zone change");
      }

      offset = new_offset;
      settings_change = true;
    }
  }

  if (json_object.containsKey("dst")) {
    bool new_dst = json_object["dst"].as<bool>();
    if (dst != new_dst) {
      if (RTC.isrunning() && !json_object.containsKey("time")) {
        if (new_dst) {
          new_time = RTC.now().unixtime() + 3600;
        } else {
          new_time = RTC.now().unixtime() - 3600;
        }
        RTC.adjust(DateTime(new_time));
        note(new_dst ? "Summer time" : "Winter time");
      }

      dst = new_dst;
      settings_change = true;
    }
  }

  if (json_object.containsKey("time")) {
    new_time = json_object["time"].as<uint32_t>() + offset + (dst ? 3600 : 0);
    if (new_time > 1546304461) {
      if (RTC.isrunning()) {
        if (abs(new_time - RTC.now().unixtime()) > 60) {
          RTC.adjust(DateTime(new_time));
        }
      } else {
        RTC.adjust(DateTime(new_time));
        note("Adjust time");
        start_time = RTC.now().unixtime() - offset - (dst ? 3600 : 0);
        if (RTC.isrunning()) {
          sayHelloToTheServer();
        }
      }
    }
  }

  if (json_object.containsKey("up")) {
    uint32_t new_update_time = json_object["up"].as<uint32_t>();
    if (update_time < new_update_time) {
      update_time = new_update_time;
    }
  }

  if (json_object.containsKey("smart")) {
    String new_smart_string = json_object["smart"].as<String>();
    if (smart_string != new_smart_string) {
      smart_string = new_smart_string;
      setSmart();
      if (smart_heating > -1) {
        heating_temperature = 0.0;
        smart_heating = -1;
        setHeating(false, "Remote");
      }
      result = "smart=" + getSmartString();
      settings_change = true;
    }
  }

  if (json_object.containsKey("val")) {
    String new_heating = json_object["val"].as<String>();
    if (strContains(new_heating, "t")) {
      heating_time = 0;
      heating_temperature = new_heating.substring(new_heating.indexOf("t") + 1).toFloat();
      downtime = 0;
      smart_heating = -1;
      remote_heating = true;
      setHeating(true, "Remote");
    }
    if (strContains(new_heating, "c")) {
      heating_time = RTC.isrunning() ? (RTC.now().unixtime() + new_heating.substring(new_heating.indexOf("c") + 1).toInt()) : new_heating.substring(new_heating.indexOf("c") + 1).toInt();
      heating_temperature = 0.0;
      downtime = 0;
      smart_heating = -1;
      remote_heating = true;
      setHeating(true, "Remote");
    }
    if (heating && strContains(new_heating.substring(0, 1), "0") && !strContains(new_heating, "t") && !strContains(new_heating, "c") && !strContains(new_heating, "v")) {
      heating_time = 0;
      heating_temperature = 0.0;
      if (smart_heating > -1) {
        downtime = downtime_plus;
      }
      smart_heating = -1;
      remote_heating = false;
      setHeating(false, "Remote");
    }
    if (strContains(new_heating, "v")) {
      vacation = new_heating.substring(new_heating.indexOf("v") + 1).toInt() + offset + (dst ? 3600 : 0);
      if (vacation > 0 && smart_heating > -1) {
        heating_time = 0;
        heating_temperature = 0.0;
        downtime = 0;
        smart_heating = -1;
        remote_heating = false;
        setHeating(false, "Vacation");
      } else {
        putOnlineData("detail", "val=" + String(heating));
      }
      per_wifi = true;
      details_change = true;
    }
  }

  if (json_object.containsKey("minimum")) {
    float new_minimum_temperature = json_object["minimum"].as<float>();
    if (minimum_temperature != new_minimum_temperature) {
      minimum_temperature = new_minimum_temperature;
      details_change = true;
    }
  }

  if (json_object.containsKey("correction")) {
    float new_correction = json_object["correction"].as<float>();
    if (correction != new_correction) {
      temperature = (temperature - correction) + new_correction;
      correction = new_correction;
      details_change = true;
    }
  }

  if (json_object.containsKey("plustemp")) {
    float new_heating_temperature_plus = json_object["plustemp"].as<float>();
    if (heating_temperature_plus != new_heating_temperature_plus) {
      heating_temperature_plus = new_heating_temperature_plus;
      details_change = true;
    }
  }

  if (json_object.containsKey("plustime")) {
    int new_heating_time_plus = json_object["plustime"].as<int>();
    if (heating_time_plus != new_heating_time_plus) {
      heating_time_plus = new_heating_time_plus;
      details_change = true;
    }
  }

  if (json_object.containsKey("downtime")) {
    int new_downtime_plus = json_object["downtime"].as<int>();
    if (downtime != new_downtime_plus) {
      downtime_plus = new_downtime_plus;
      details_change = true;
    }
  }

  if (settings_change || details_change) {
    note("Received the data:\n " + payload);
    saveSettings();
  }
  if (per_wifi && (result.length() > 0 || details_change)) {
    if (details_change) {
      result += String(result.length() > 0 ? "&" : "") + "detail=" + getThermostatDetail();
    }
    putOnlineData("detail", result);
  }
}

void setSmart() {
  if (smart_string.length() < 2) {
    smart_count = 0;
    return;
  }

  String smart;
  bool enabled;
  String days;
  float temp;
  int start_time;
  int end_time;

  int count = 1;
  smart_count = 1;
  for (byte b: smart_string) {
    if (b == ',') {
      count++;
    }
    if (b == 't') {
      smart_count++;
    }
  }

  if (smart_array != 0) {
    delete [] smart_array;
  }
  smart_array = new Smart[smart_count];
  smart_count = 0;

  for (int i = 0; i < count; i++) {
    smart = get1(smart_string, i);
    if (smart.length() > 0 && strContains(smart, "t")) {
      enabled = !strContains(smart, "/");
      smart = enabled ? smart : smart.substring(1);

      start_time = strContains(smart, "_") ? smart.substring(0, smart.indexOf("_")).toInt() : -1;
      end_time = strContains(smart, "-") ? smart.substring(smart.indexOf("-") + 1).toInt() : -1;

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

      smart_array[smart_count++] = (Smart) {enabled, days, temp, start_time, end_time};
    }
  }
}

bool automaticSettings() {
  return automaticSettings(hasTheTemperatureChanged());
}

bool automaticSettings(bool temperature_changed) {
  bool result = false;
  bool new_heating = false;
  float new_heating_temperature = 0.0;
  DateTime now = RTC.now();

  if (RTC.isrunning()) {
    int current_time = (now.hour() * 60) + now.minute();

    if (current_time == 120 || current_time == 180) {
      if (now.month() == 3 && now.day() > 24 && days_of_the_week[now.dayOfTheWeek()][0] == 's' && current_time == 120 && !dst) {
        int new_time = RTC.now().unixtime() + 3600;
        RTC.adjust(DateTime(new_time));
        dst = true;
        saveSettings();
        note("Smart set to summer time");
      }
      if (now.month() == 10 && now.day() > 24 && days_of_the_week[now.dayOfTheWeek()][0] == 's' && current_time == 180 && dst) {
        int new_time = RTC.now().unixtime() - 3600;
        RTC.adjust(DateTime(new_time));
        dst = false;
        saveSettings();
        note("Smart set to winter time");
      }
    }
  }

  if (heating_time == 0) {
    if (temperature < minimum_temperature && heating_temperature < minimum_temperature) {
      result = true;
      new_heating = true;
      new_heating_temperature = minimum_temperature;
      smart_heating = -2;
    }

    if (downtime == 0 && (vacation == 0 || (RTC.isrunning() && vacation < now.unixtime()))) {
      int i = -1;
      while (++i < smart_count) {
        if (smart_array[i].enabled) {
          bool inTime = false;

          if (strContains(smart_array[i].days, "w") || (RTC.isrunning() && strContains(smart_array[i].days, days_of_the_week[now.dayOfTheWeek()]))) {

            int current_time = RTC.isrunning() ? ((now.hour() * 60) + now.minute()) : -1;
            inTime = ((smart_array[i].start_time == -1 || (RTC.isrunning() && smart_array[i].start_time <= current_time)) && (smart_array[i].end_time == -1 || (RTC.isrunning() && smart_array[i].end_time > current_time)));
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
            if (RTC.isrunning() && smart_heating == i) {
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
    setHeating(new_heating, "Smart");

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
  putOnlineData("detail", "val=" + String(set) + "&returned=" + String(temperature) + (heating_time > 0 ? "c" + String(getHeatingTime()) : "") + (heating_temperature > 0 ? "t" + String(heating_temperature) : ""));

  if (set) {
    saveTheState();
  } else {
    if (LittleFS.exists("/resume.txt")) {
      LittleFS.remove("/resume.txt");
    }
  }

  delay(500);
}
