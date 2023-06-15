#include "core.h"

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  LittleFS.begin();
  Wire.begin();

  keep_log = LittleFS.exists("/log.txt");

  #ifdef physical_clock
    rtc.begin();
    note("iDom Thermostat " + String(version) + "." + String(core_version));
  #else
    note("iDom Thermostat " + String(version) + "." + String(core_version) + "wo");
  #endif

  sprintf(host_name, "therm_%s", String(WiFi.macAddress()).c_str());
  WiFi.hostname(host_name);

  pinMode(relay_pin, OUTPUT);
  digitalWrite(relay_pin, LOW);

  if (!readSettings(0)) {
    delay(1000);
    readSettings(1);
  }

  sensors.begin();
  sensors.requestTemperatures();
  temperature = sensors.getTempCByIndex(0) + correction;
  if (!resume()) {
    smartAction(6, false);
  };

  if (RTCisrunning()) {
    start_u_time = rtc.now().unixtime() - offset - (dst ? 3600 : 0);
  }

  powerButton.setSingleClickCallback(&powerButtonSingle, (void*)"");
  powerButton.setLongPressCallback(&powerButtonLong, (void*)"");

  setupOTA();
  connectingToWifi(false);
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    ArduinoOTA.handle();
    server.handleClient();
    MDNS.update();
  } else {
    if (!auto_reconnect) {
      connectingToWifi(true);
    }
  }

  powerButton.poll();

  if (hasTimeChanged()) {
    if (downtime > 0) {
      downtime--;
    }

    if (heating) {
      if (heating_time > 0) {
        saveTheState();
        if ((RTCisrunning() ? (heating_time - rtc.now().unixtime()) : heating_time--) <= 0) {
          automaticHeatingOff();
        }
      }
      if (heating_temperature > 0.0 && heating_temperature <= temperature) {
        automaticHeatingOff();
      }
    }
    automation();
  }
}


bool readSettings(bool backup) {
  File file = LittleFS.open(backup ? "/backup.txt" : "/settings.txt", "r");
  if (!file) {
    note("The " + String(backup ? "backup" : "settings") + " file cannot be read");
    return false;
  }

  DynamicJsonDocument json_object(1024);
  DeserializationError deserialization_error = deserializeJson(json_object, file);

  if (deserialization_error) {
    note(String(backup ? "Backup" : "Settings") + " error: " + String(deserialization_error.f_str()));
    file.close();
    return false;
  }

  file.seek(0);
  note("Reading the " + String(backup ? "backup" : "settings") + " file:\n " + file.readString());
  file.close();

  if (json_object.containsKey("log")) {
    last_accessed_log = json_object["log"].as<int>();
  }
  if (json_object.containsKey("ssid")) {
    ssid = json_object["ssid"].as<String>();
  }
  if (json_object.containsKey("password")) {
    password = json_object["password"].as<String>();
  }
  if (json_object.containsKey("uprisings")) {
    uprisings = json_object["uprisings"].as<int>() + 1;
  }
  if (json_object.containsKey("offset")) {
    offset = json_object["offset"].as<int>();
  }
  dst = json_object.containsKey("dst");
  if (json_object.containsKey("smart")) {
    if (json_object.containsKey("ver")) {
      setSmart(json_object["smart"].as<String>());
    } else {
      setSmart(oldSmart2NewSmart(json_object["smart"].as<String>()));
    }
  }
  smart_lock = json_object.containsKey("smart_lock");
  if (json_object.containsKey("location")) {
    geo_location = json_object["location"].as<String>();
    if (geo_location.length() > 2) {
      sun.setPosition(geo_location.substring(0, geo_location.indexOf("x")).toDouble(), geo_location.substring(geo_location.indexOf("x") + 1).toDouble(), 0);
    }
  }
  if (json_object.containsKey("sunset")) {
    sunset_u_time = json_object["sunset"].as<int>();
  }
  if (json_object.containsKey("sunrise")) {
    sunrise_u_time = json_object["sunrise"].as<int>();
  }
  sensor_twilight = json_object.containsKey("sensor_twilight");
  calendar_twilight = json_object.containsKey("twilight");
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
  key_lock = json_object.containsKey("key_lock");

  saveSettings(false);

  return true;
}

void saveSettings() {
  saveSettings(true);
}

void saveSettings(bool log) {
  DynamicJsonDocument json_object(1024);

  json_object["ver"] = String(version) + "." + String(core_version);
  if (last_accessed_log > 0) {
    json_object["log"] = last_accessed_log;
  }
  if (ssid.length() > 0) {
    json_object["ssid"] = ssid;
  }
  if (password.length() > 0) {
    json_object["password"] = password;
  }
  json_object["uprisings"] = uprisings;
  if (offset > 0) {
    json_object["offset"] = offset;
  }
  if (dst) {
    json_object["dst"] = dst;
  }
  if (smart_count > 0) {
    json_object["smart"] = getSmartString(true);
  }
  if (smart_lock) {
    json_object["smart_lock"] = smart_lock;
  }
  if (geo_location != default_location) {
    json_object["location"] = geo_location;
  }
  if (sunset_u_time > 0) {
    json_object["sunset"] = sunset_u_time;
  }
  if (sunrise_u_time > 0) {
    json_object["sunrise"] = sunrise_u_time;
  }
  if (sensor_twilight) {
    json_object["sensor_twilight"] = sensor_twilight;
  }
  if (calendar_twilight) {
    json_object["twilight"] = calendar_twilight;
  }
  if (correction != default_correction) {
    json_object["correction"] = correction;
  }
  if (minimum_temperature != default_minimum_temperature) {
    json_object["minimum"] = minimum_temperature;
  }
  if (heating_temperature_plus != default_heating_temperature_plus) {
    json_object["plustemp"] = heating_temperature_plus;
  }
  if (heating_time_plus != default_heating_time_plus) {
    json_object["plustime"] = heating_time_plus;
  }
  if (downtime_plus != default_downtime_plus) {
    json_object["downtime"] = downtime_plus;
  }
  if (vacation > 0) {
    json_object["vacation"] = vacation;
  }
  if (key_lock) {
    json_object["key_lock"] = key_lock;
  }

  if (writeObjectToFile("settings", json_object)) {
    if (log) {
      String log_text;
      serializeJson(json_object, log_text);
      note("Saving settings:\n " + log_text);
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

  StaticJsonDocument<100> json_object;
  DeserializationError deserialization_error = deserializeJson(json_object, file);
  file.close();

  if (deserialization_error) {
    note("Resume error: " + String(deserialization_error.c_str()));
    return false;
  }

  heating = json_object.containsKey("heating");
  if (json_object.containsKey("htemp")) {
    heating_temperature = json_object["htemp"].as<float>();
  }
  if (json_object.containsKey("htime")) {
    heating_time = json_object["htime"].as<int>();
    if (getHeatingTime() > 4000 && !RTCisrunning()) {
      heating_time = 0;
    }
  }

  if (heating || heating_temperature > 0.0 || heating_time > 0) {
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
  StaticJsonDocument<100> json_object;

  if (heating) {
    json_object["heating"] = heating;
  }
  if (heating_temperature > 0.0) {
    json_object["htemp"] = heating_temperature;
  }
  if (heating_time > 0) {
    json_object["htime"] = heating_time;
  }

  writeObjectToFile("resume", json_object);
}


String getValue() {
  return String(heating);
}

int getHeatingTime() {
  return heating_time > 0 ? (RTCisrunning() ? (heating_time - rtc.now().unixtime()) : heating_time) : 0;
}

void startServices() {
  server.on("/hello", HTTP_POST, handshake);
  server.on("/set", HTTP_PUT, receivedOfflineData);
  server.on("/state", HTTP_GET, requestForState);
  server.on("/basicdata", HTTP_POST, exchangeOfBasicData);
  server.on("/log", HTTP_GET, requestForLogs);
  server.on("/log", HTTP_DELETE, clearTheLog);
  server.on("/test/smartdetail", HTTP_GET, getSmartDetail);
  server.on("/test/smartdetail/raw", HTTP_GET, getRawSmartDetail);
  server.on("/admin/log", HTTP_POST, activationTheLog);
  server.on("/admin/log", HTTP_DELETE, deactivationTheLog);
  server.begin();

  note(String(host_name) + (MDNS.begin(host_name) ? " started" : " unsuccessful!"));

  MDNS.addService("idom", "tcp", 8080);

  ntpClient.begin();
  ntpClient.update();
  readData("{\"time\":" + String(ntpClient.getEpochTime()) + "}", false);
  getOfflineData();
}

void handshake() {
  if (server.hasArg("plain")) {
    readData(server.arg("plain"), true);
  }

  String reply = "\"id\":\"" + WiFi.macAddress() + "\"";
  reply += ",\"version\":" + String(version) + "." + String(core_version);
  reply += ",\"offline\":true";
  if (keep_log) {
    reply += ",\"last_accessed_log\":" + String(last_accessed_log);
  }
  if (start_u_time > 0) {
    reply += ",\"start\":" + String(start_u_time);
  } else {
    reply += ",\"active\":" + String(millis() / 1000);
  }
  reply += ",\"uprisings\":" + String(uprisings);
  if (offset > 0) {
    reply += ",\"offset\":" + String(offset);
  }
  if (dst) {
    reply += ",\"dst\":true";
  }
  if (RTCisrunning()) {
    #ifdef physical_clock
      reply += ",\"rtc\":true";
    #endif
    reply += ",\"time\":" + String(rtc.now().unixtime() - offset - (dst ? 3600 : 0));
  }
  if (smart_count > 0) {
    reply += ",\"smart\":\"" + getSmartString(true) + "\"";
  }
  if (smart_lock) {
    reply += ",\"smart_lock\":true";
  }
  if (geo_location.length() > 2) {
    reply += ",\"location\":\"" + geo_location + "\"";
  }
  if (last_sun_check > -1) {
    reply += ",\"sun_check\":" + String(last_sun_check);
  }
  if (next_sunset > -1) {
    reply += ",\"next_sunset\":" + String(next_sunset);
  }
  if (next_sunrise > -1) {
    reply += ",\"next_sunrise\":" + String(next_sunrise);
  }
  if (sunset_u_time > 0) {
    reply += ",\"sunset\":" + String(sunset_u_time);
  }
  if (sunrise_u_time > 0) {
    reply += ",\"sunrise\":" + String(sunrise_u_time);
  }
  if (temperature > -127.0) {
    reply += ",\"temp\":" + String(temperature);
  }
  if (sensor_twilight) {
    reply += ",\"sensor_twilight\":true";
  }
  if (calendar_twilight) {
    reply += ",\"twilight\":true";
  }
  if (correction != default_correction) {
    reply += ",\"correction\":" + String(correction);
  }
  if (minimum_temperature != default_minimum_temperature) {
    reply += ",\"minimum\":" + String(minimum_temperature);
  }
  if (heating_temperature_plus != default_heating_temperature_plus) {
    reply += ",\"plustemp\":" + String(heating_temperature_plus);
  }
  if (heating_time_plus != default_heating_time_plus) {
    reply += ",\"plustime\":" + String(heating_time_plus);
  }
  if (downtime_plus != default_downtime_plus) {
    reply += ",\"downtime\":" + String(downtime_plus);
  }
  if (vacation > 0) {
    reply += ",\"vacation\":" + String(vacation);
  }
  if (getValue() != "0") {
    reply += "\",\"value\":" + getValue();
  }
  if (heating_temperature > 0.0) {
    reply += ",\"htemp\":" + String(heating_temperature);
  }
  if (heating_time > 0) {
    reply += ",\"htime\":" + String(getHeatingTime());
  }
  if (key_lock) {
    reply += ",\"key_lock\":true";
  }

  Serial.print("\nHandshake");
  server.send(200, "text/plain", "{" + reply + "}");
}

void requestForState() {
  String reply = "\"value\":" + getValue();

  if (heating_temperature > 0.0) {
    reply += ",\"htemp\":" + String(heating_temperature);
  }
  if (heating_time > 0) {
    reply += ",\"htime\":" + String(getHeatingTime());
  }

  if (temperature > -127.0) {
    reply += ",\"temp\":" + String(temperature);
  }

  server.send(200, "text/plain", "{" + reply + "}");
}

void exchangeOfBasicData() {
  if (server.hasArg("plain")) {
    readData(server.arg("plain"), true);
  }

  String reply = "\"ip\":\"" + WiFi.localIP().toString() + "\"" + ",\"id\":\"" + WiFi.macAddress() + "\"";

  reply += ",\"offset\":" + String(offset) + ",\"dst\":" + String(dst);

  if (RTCisrunning()) {
    reply += ",\"time\":" + String(rtc.now().unixtime() - offset - (dst ? 3600 : 0));
  }

  if (temperature > -127.0) {
    reply +=  ",\"temp\":" + String(temperature);
  }

  server.send(200, "text/plain", "{" + reply + "}");
}


void powerButtonSingle(void* b) {
  if (key_lock) {
    return;
  }
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

void powerButtonLong(void* b) {
  if (key_lock) {
    return;
  }

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

void readData(const String& payload, bool per_wifi) {
  DynamicJsonDocument json_object(1024);
  DeserializationError deserialization_error = deserializeJson(json_object, payload);

  if (deserialization_error) {
    note("Read data error: " + String(deserialization_error.c_str()) + "\n" + payload);
    return;
  }

  bool settings_change = false;
  bool twilight_change = false;

  if (json_object.containsKey("ip") && json_object.containsKey("id")) {
      for (int i = 0; i < devices_count; i++) {
        if (devices_array[i].ip == json_object["ip"].as<String>()) {
          devices_array[i].mac = json_object["id"].as<String>();
        }
      }
  }

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
    if (dst != strContains(json_object["dst"].as<String>(), 1)) {
      dst = !dst;
      settings_change = true;
      if (RTCisrunning() && !json_object.containsKey("time")) {
        rtc.adjust(DateTime(rtc.now().unixtime() + (dst ? 3600 : -3600)));
        note(dst ? "Summer time" : "Winter time");
      }
    }
  }

  if (json_object.containsKey("time")) {
    int new_u_time = json_object["time"].as<int>() + offset + (dst ? 3600 : 0);
    if (new_u_time > 1546304461) {
      if (RTCisrunning()) {
        if (abs(new_u_time - (int)rtc.now().unixtime()) > 60) {
          rtc.adjust(DateTime(new_u_time));
          note("Adjust time");
        }
      } else {
        #ifdef physical_clock
          rtc.adjust(DateTime(new_u_time));
        #else
          rtc.begin(DateTime(new_u_time));
        #endif
        note("RTC begin");
        start_u_time = (millis() / 1000) + rtc.now().unixtime() - offset - (dst ? 3600 : 0);
      }
    }
  }

  if (json_object.containsKey("smart")) {
    if (getSmartString(true) != json_object["smart"].as<String>()) {
      setSmart(json_object["smart"].as<String>());
      if (smart_heating > -1) {
        heating_temperature = 0.0;
        smart_heating = -1;
        setHeating(false, per_wifi ? (json_object.containsKey("apk") ? "apk" : "local") : "cloud");
      }
      if (selector_counter > 0) {
        selector_counter = 1;
      }
      settings_change = true;
    }
  }

  if (json_object.containsKey("smart_lock")) {
    if (smart_lock != strContains(json_object["smart_lock"].as<String>(), 1)) {
      smart_lock = !smart_lock;
      settings_change = true;
    }
  }

  if (json_object.containsKey("location")) {
    if (geo_location != json_object["location"].as<String>()) {
      geo_location = json_object["location"].as<String>();
      if (geo_location.length() > 2) {
        sun.setPosition(geo_location.substring(0, geo_location.indexOf("x")).toDouble(), geo_location.substring(geo_location.indexOf("x") + 1).toDouble(), 0);
      } else {
        last_sun_check = -1;
        next_sunset = -1;
        next_sunrise = -1;
        sunset_u_time = 0;
        sunrise_u_time = 0;
        calendar_twilight = false;
      }
      settings_change = true;
    }
  }

  if (json_object.containsKey("correction")) {
    if (correction != json_object["correction"].as<float>()) {
      temperature = (temperature - correction) + json_object["correction"].as<float>();
      correction = json_object["correction"].as<float>();
      settings_change = true;
    }
  }

  if (json_object.containsKey("minimum")) {
    if (minimum_temperature != json_object["minimum"].as<float>()) {
      minimum_temperature = json_object["minimum"].as<float>();
      settings_change = true;
    }
  }

  if (json_object.containsKey("plustemp")) {
    if (heating_temperature_plus != json_object["plustemp"].as<float>()) {
      heating_temperature_plus = json_object["plustemp"].as<float>();
      settings_change = true;
    }
  }

  if (json_object.containsKey("plustime")) {
    if (heating_time_plus != json_object["plustime"].as<int>()) {
      heating_time_plus = json_object["plustime"].as<int>();
      settings_change = true;
    }
  }

  if (json_object.containsKey("downtime")) {
    if (downtime != json_object["downtime"].as<int>()) {
      downtime_plus = json_object["downtime"].as<int>();
      settings_change = true;
    }
  }

  if (json_object.containsKey("key_lock")) {
    if (key_lock != strContains(json_object["key_lock"].as<String>(), 1)) {
      key_lock = !key_lock;
      settings_change = true;
    }
  }

  if (json_object.containsKey("light")) {
    if (sensor_twilight != strContains(json_object["light"].as<String>(), "t")) {
      sensor_twilight = !sensor_twilight;
      twilight_change = true;
      settings_change = true;
      if (RTCisrunning()) {
        int current_time = (rtc.now().hour() * 60) + rtc.now().minute();
        if (sensor_twilight) {
          if (abs(current_time - dusk_time) > 60) {
            dusk_time = current_time;
          }
        } else {
          if (abs(current_time - dawn_time) > 60) {
            dawn_time = current_time;
          }
        }
      }
    }
    if (strContains(json_object["light"].as<String>(), "t")) {
		  light_sensor = json_object["light"].as<String>().substring(0, json_object["light"].as<String>().indexOf("t")).toInt();
    } else {
		  light_sensor = json_object["light"].as<int>();
    }
  }

  if (json_object.containsKey("vacation")) {
    if (vacation != json_object["vacation"].as<uint32_t>()) {
      vacation = json_object["vacation"].as<uint32_t>() + offset + (dst ? 3600 : 0);
      if (vacation > 0 && smart_heating > -1 && (RTCisrunning() && vacation < rtc.now().unixtime())) {
        heating_time = 0;
        heating_temperature = 0.0;
        downtime = 0;
        smart_heating = -1;
        remote_heating = false;
        setHeating(false, "vacation");
      }
      settings_change = true;
    }
  }

  if (json_object.containsKey("val")) {
    String newValue = json_object["val"].as<String>();

    if (strContains(newValue, "t")) {
      heating_time = 0;
      heating_temperature = newValue.substring(newValue.indexOf("t") + 1).toFloat();
      downtime = 0;
      smart_heating = -1;
      remote_heating = true;
      setHeating(true, per_wifi ? (json_object.containsKey("apk") ? "apk" : "local") : "cloud");
    }
    if (strContains(newValue, "c")) {
      heating_time = RTCisrunning() ? (rtc.now().unixtime() + newValue.substring(newValue.indexOf("c") + 1).toInt()) : newValue.substring(newValue.indexOf("c") + 1).toInt();
      heating_temperature = 0.0;
      downtime = 0;
      smart_heating = -1;
      remote_heating = true;
      setHeating(true, per_wifi ? (json_object.containsKey("apk") ? "apk" : "local") : "cloud");
    }
    if (heating && strContains(newValue.substring(0, 1), "0") && !strContains(newValue, "t") && !strContains(newValue, "c") && !strContains(newValue, "v")) {
      heating_time = 0;
      heating_temperature = 0.0;
      if (smart_heating > -1) {
        downtime = downtime_plus;
      }
      smart_heating = -1;
      remote_heating = false;
      setHeating(false, per_wifi ? (json_object.containsKey("apk") ? "apk" : "local") : "cloud");
    }
  }

  if (settings_change) {
    note("Received the data:\n " + payload);
    saveSettings();
  }
  if (json_object.containsKey("light")) {
    smartAction(0, twilight_change);
  }
  if (json_object.containsKey("location") && RTCisrunning()) {
    getSunriseSunset(rtc.now());
  }
}

void automation() {
  if (!RTCisrunning()) {
    smartAction();
    return;
  }

  DateTime now = rtc.now();
  int current_time = (now.hour() * 60) + now.minute();

  if (now.second() == 0) {
    if (current_time == 60) {
      ntpClient.update();
      readData("{\"time\":" + String(ntpClient.getEpochTime()) + "}", false);

      if (last_accessed_log++ > 14) {
        deactivationTheLog();
      }
    }
  }

  if (current_time == 120 || current_time == 180) {
    if (now.month() == 3 && now.day() > 24 && days_of_the_week[now.dayOfTheWeek()][0] == 's' && current_time == 120 && !dst) {
      int new_u_time = now.unixtime() + 3600;
      rtc.adjust(DateTime(new_u_time));
      dst = true;
      note("Setting summer time");
      saveSettings();
      getSunriseSunset(now);
    }
    if (now.month() == 10 && now.day() > 24 && days_of_the_week[now.dayOfTheWeek()][0] == 's' && current_time == 180 && dst) {
      int new_u_time = now.unixtime() - 3600;
      rtc.adjust(DateTime(new_u_time));
      dst = false;
      note("Setting winter time");
      saveSettings();
      getSunriseSunset(now);
    }
  }

  if (geo_location.length() < 2) {
    if (current_time == 181) {
      smart_lock = false;
      saveSettings();
    }
  } else {
    if (now.second() == 0 && ((current_time > 181 && last_sun_check != now.day()) || next_sunset == -1 || next_sunrise == -1)) {
      getSunriseSunset(now);
    }

    if (next_sunset > -1 && next_sunrise > -1) {
      if ((!calendar_twilight && current_time == next_sunset) || (calendar_twilight && current_time == next_sunrise)) {
        if (current_time == next_sunset) {
          calendar_twilight = true;
          sunset_u_time = now.unixtime() - offset - (dst ? 3600 : 0);
        }
        if (current_time == next_sunrise) {
          calendar_twilight = false;
          sunrise_u_time = now.unixtime() - offset - (dst ? 3600 : 0);
        }
        smart_lock = false;
        saveSettings();
      }
    }
  }

  smartAction();
}

bool hasTheTemperatureChanged() {
  if (loop_u_time % 60 != 0) {
    return -1;
  }

  sensors.requestTemperatures();
  float new_temperature = sensors.getTempCByIndex(0) + correction;

  if (temperature != new_temperature) {
    temperature = new_temperature;
    putMultiOfflineData("{\"temp\":" + String(temperature) + "}");
    return 6;
  }

  return -1;
}

void smartAction() {
  smartAction(hasTheTemperatureChanged(), false);
}


void automaticHeatingOff() {
  heating_time = 0;
  heating_temperature = 0.0;
  smart_heating = -1;
  remote_heating = false;
  setHeating(false, "automatic");
}

void setHeating(bool set, String orderer) {
  if (heating != set) {
    heating = set;
    digitalWrite(relay_pin, set);
  }

  note(orderer + " heating " + (set ? (heating_time == 0 && heating_temperature == 0.0 ? "on" : ((heating_time > 0 ? "on time " + String(getHeatingTime()) : "") + (heating_temperature > 0.0 ? "by temperature " + String(heating_temperature) : ""))) : "off"));

  if (set) {
    saveTheState();
  } else {
    if (LittleFS.exists("/resume.txt")) {
      LittleFS.remove("/resume.txt");
    }
  }

  delay(500);
}
