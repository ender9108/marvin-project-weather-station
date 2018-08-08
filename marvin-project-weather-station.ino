#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <ESP8266TrueRandom.h>

#define SEALEVELPRESSURE_HPA (1013.25)

struct Config {
  char wifiSsid[32];
  char wifiPassword[64];
  char mqttHost[128];
  int  mqttPort;
  char mqttUsername[32];
  char mqttPassword[64];
  char mqttPublishChannel[128];
  char uuid[64];
};

Config config;
ESP8266WebServer server(80);
WiFiClient wifiClient;
PubSubClient client(wifiClient);
Adafruit_BME280 bme; // I2C

const char* wifiSsid        = "marvin-weather-station";
const char* wifiPassword    = "marvin-weather-station";
bool wifiConnected          = false;
bool mqttConnected          = false;
const float voltMax         = 4.2;
const float voltMin         = 3.0;

void setup() {
    Serial.begin(9600);

    EEPROM.begin(512);
    clearEeprom();

    // Get wifi SSID and PASSW from eeprom
    getParametersFromEeprom();

    if (true == checkEepromValues()) {
      wifiConnected = wifiConnect();

      if (true == wifiConnected) {
        mqttConnected = mqttConnect();
      }
    }

    if (false == wifiConnected) {
      WiFi.mode(WIFI_AP);
      WiFi.softAP(wifiSsid, wifiPassword);
      Serial.print("WiFi AP is ready (IP : ");  
      Serial.print(WiFi.softAPIP());
      Serial.println(")");

      server.on("/", httpParametersPage);
      server.on("/save", httpSaveWifiInfoToEeprom);
      server.on("/restart", httpRestartEsp);
      server.begin();

      Serial.println("Webserver is ready");
      Serial.print("http://");
      Serial.print(WiFi.softAPIP());
      Serial.println("/");
    } else {
      if (!bme.begin(0x76)) {
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
        while (1);
        }

      bme.setSampling(Adafruit_BME280::MODE_NORMAL,
                      Adafruit_BME280::SAMPLING_X1, // temperature
                      Adafruit_BME280::SAMPLING_X1, // pressure
                      Adafruit_BME280::SAMPLING_X1, // humidity
                      Adafruit_BME280::FILTER_OFF);
    
      client.loop();

      String payload;
      float voltage = readBatteryVoltage();
      float voltPercent = voltToPercent(voltage);
      voltPercent = round(voltPercent);

      if (voltage <= 3.2) {
        payload = "{\"battery\":" + String(voltPercent, 0) + ", \"message\":\"Battery low voltage.\",\"name\":\"marvin-weather-station\",\"uuid\":\"" + config.uuid + "\"}";
        publishMqttMessage(config.mqttPublishChannelBat, payload);
      }

      payload = "{\"battery\":" + String(voltPercent, 0) + ",\"name\":\"marvin-weather-station\",\"uuid\":\"" + config.uuid + "\",\"temperature\":{\"value\":" + String(bme.readTemperature(),2) + ",\"unit\":\"C\"},humidity:{\"value\":" + String(bme.readHumidity(),2) + ",\"unit\":\"%\"},pressure:{\"value\":" + String((bme.readPressure() / 100.0F),0) + ", \"unit\":\"hPa\"},altitude:{\"value\":" + String(bme.readAltitude(SEALEVELPRESSURE_HPA),0) + ",\"unit\":\"m\"}}";
      publishMqttMessage(config.mqttPublishChannel, payload);

      // 3600e6 = 1 heure
      // ESP.deepSleep(3600e6);
      // to debug deep sleep = 10s
      // ESP.deepSleep(10e6);
    }
}

void loop() {
  if (false == wifiConnected) {
    server.handleClient();
  }
}

void httpParametersPage() {
  /*
   * @todo gestion des erreurs
   */
  
  String response = "";
  response  = "\r\n\r\n<!DOCTYPE HTML>\r\n<html><body>";
  response += "<h1>Marvin weather station</h1>";
  response += "<h3>Wifi parameters</h3>";
  response += "<form method=\"GET\" action=\"/save\">";
  response += "<p><label style=\"min-width:90px;\" for=\"wssid\">Ssid : </label><input maxlength=\"32\" type=\"text\" name=\"wssid\" id=\"wssid\" placeholder=\"SSID (see on your box)\" style=\"border:1px solid #000;width:250px;\"><p>";
  response += "<p><label style=\"min-width:90px;\" for=\"wpassw\">Passord : </label><input maxlength=\"64\" type=\"password\" name=\"wpassw\" id=\"wpassw\" style=\"border:1px solid #000;width:250px;\"></p>";
  response += "<hr>";
  response += "<h3>Mqtt parameters</h3>";
  response += "<p><label style=\"min-width:90px;\" for=\"wmqttHost\">Host / Ip  : </label><input maxlength=\"128\" type=\"text\" name=\"wmqttHost\" id=\"wmqttHost\" style=\"border:1px solid #000;width:250px;\"><p>";
  response += "<p><label style=\"min-width:90px;\" for=\"wmqttPort\">Port       : </label><input maxlength=\"6\" type=\"text\" name=\"wmqttPort\" id=\"wmqttPort\" value=\"1883\" style=\"border:1px solid #000;width:250px;\"></p>";
  response += "<p><label style=\"min-width:90px;\" for=\"wmqttUser\">Username   : </label><input maxlength=\"32\" type=\"text\" name=\"wmqttUser\" id=\"wmqttUser\" style=\"border:1px solid #000;width:250px;\"><p>";
  response += "<p><label style=\"min-width:90px;\" for=\"wmqttPass\">Passord    : </label><input maxlength=\"64\" type=\"text\" name=\"wmqttPass\" id=\"wmqttPass\" style=\"border:1px solid #000;width:250px;\"></p>";
  response += "<p><label style=\"min-width:90px;\" for=\"wmqttChan\">Channel    : </label><input maxlength=\"128\" type=\"text\" name=\"wmqttChan\" id=\"wmqttChan\" style=\"border:1px solid #000;width:250px;\"></p>";
  response += "<hr>";
  response += "<p><input type=\"submit\" value=\"Save\"></p>";
  response += "</form>";
  response += "</body></html>\r\n\r\n";
  server.send(200, "text/html", response);
}

void httpRestartEsp() {
  String response = "";
  response  = "\r\n\r\n<!DOCTYPE HTML>\r\n<html><body>";
  response += "<h1>Marvin weather station</h1>";
  response += "<h3>Restart in progress...</h3>";
  response += "<p>After restarting this page will no longer be available</p>";
  response += "</body></html>\r\n\r\n";
  server.send(200, "text/html", response);

  delay(5000);
  
  ESP.restart();
}

void httpSaveWifiInfoToEeprom() {
  String response = "";
  bool error = false;
  
  if (!server.hasArg("wssid") || !server.hasArg("wpassw")){  
    error = true;
    // display error
  }

  if (server.arg("wssid").length() > 1 && server.arg("wpassw").length() > 1) {
    error = true;
    // display error
  }

  if (false == error) {
    setParametersToEeprom(
      server.arg("wssid"),
      server.arg("wpassw"),
      server.arg("wmqttHost"),
      server.arg("wmqttPort"),
      server.arg("wmqttUser"),
      server.arg("wmqttPass"),
      server.arg("wmqttChan")
    );

    response  = "\r\n\r\n<!DOCTYPE HTML>\r\n<html><body>";
    response += "<h1>Marvin weather station</h1>";
    response += "<p>OK - Wifi parameters has been saved !</p>";
    response += "<p>OK - MQTT parameters has been saved !</p>";
    response += "<p>Please restart module to apply new parameters.</p>";
    response += "<p><a href=\"/restart\">RESTART</a></p>";
    response += "</body></html>\r\n\r\n";
    server.send(200, "text/html", response);
  } else {
    httpRedirect("/");
  }
}

float readBatteryVoltage() {
  float sensorValue = analogRead(A0);
  float voltage = sensorValue / 1023;
  voltage       = 4.2 * voltage;

  return voltage;
}

float voltToPercent(float voltage) {
  float voltPercent = (voltage / (voltMax - voltMin)) * 100;
  return voltPercent;
}

void publishMqttMessage(char* channel, String payload) {
    if (false == client.publish(channel, payload.c_str())) {
        Serial.println("Publish fail !");
    }
}

bool wifiConnect() {
  unsigned int count = 0;
  WiFi.begin(config.wifiSsid, config.wifiPassword);
  Serial.print("Try to connect to ");
  Serial.println(config.wifiSsid);

  while (count < 20) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("");
      Serial.print("WiFi connected (IP : ");  
      Serial.print(WiFi.localIP());
      Serial.println(")");
  
      return true;
    } else {
      delay(500);
      Serial.print(".");  
    }

    count++;
  }

  Serial.print("Error connection to ");
  Serial.println(config.wifiSsid);
  return false;
}

bool mqttConnect() {
    client.setServer(config.mqttHost, config.mqttPort);
    int count = 0;

    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        if (client.connect("MarvinWeatherStation", config.mqttUsername, config.mqttPassword)) {
            Serial.println("connected");
            return true;
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);

            if (count == 10) {
              return false;
            }
        }

        count++;
    }

    return false;
}

void getParametersFromEeprom() {
  Serial.println("Reading wifi ssid from EEPROM");

  for (int i = 0; i < 32; ++i)
  {
    if (EEPROM.read(i) != 0) {
      config.wifiSsid[i] = char(EEPROM.read(i));
    }
  }

  Serial.print("SSID value (length: ");
  Serial.print(strlen(config.wifiSsid));
  Serial.print("): ");
  Serial.println(config.wifiSsid);


  Serial.println("Reading wifi password from EEPROM");

  for (int i = 32; i < 96; ++i)
  {
    if (EEPROM.read(i) != 0) {
      config.wifiPassword[i] = char(EEPROM.read(i));
    }
  }

  Serial.print("Password value (length: ");
  Serial.print(strlen(config.wifiPassword));
  Serial.print("): ");
  Serial.println(config.wifiPassword);


  Serial.println("Reading mqtt host from EEPROM");

  for (int i = 96; i < 224; ++i)
  {
    if (EEPROM.read(i) != 0) {
      config.mqttHost[i] = char(EEPROM.read(i));
    }
  }

  Serial.print("IP value (length: ");
  Serial.print(strlen(config.mqttHost));
  Serial.print("): ");
  Serial.println(config.mqttHost);


  Serial.println("Reading mqtt port from EEPROM");
  char tmpPort[15];

  for (int i = 224; i < 230; ++i)
  {
    if (EEPROM.read(i) != 0) {
      tmpPort[i] = char(EEPROM.read(i));
    }
  }

  config.mqttPort = atoi(tmpPort);

  Serial.print("Port value : ");
  Serial.println(config.mqttPort);
  

  Serial.println("Reading mqtt username from EEPROM");

  for (int i = 230; i < 262; ++i)
  {
    if (EEPROM.read(i) != 0) {
      config.mqttUsername[i] = char(EEPROM.read(i));
    }
  }

  Serial.print("Username value (length: ");
  Serial.print(strlen(config.mqttUsername));
  Serial.print("): ");
  Serial.println(config.mqttUsername);
  

  Serial.println("Reading mqtt password from EEPROM");

  for (int i = 262; i < 326; ++i)
  {
    if (EEPROM.read(i) != 0) {
      config.mqttPassword[i] = char(EEPROM.read(i));
    }
  }

  Serial.print("Password value (length: ");
  Serial.print(strlen(config.mqttPassword));
  Serial.print("): ");
  Serial.println(config.mqttPassword);


  Serial.println("Reading mqtt channel from EEPROM");

  for (int i = 326; i < 454; ++i)
  {
    if (EEPROM.read(i) != 0) {
      config.mqttPublishChannel[i] = char(EEPROM.read(i));
    }
  }

  Serial.print("Channel value (length: ");
  Serial.print(strlen(config.mqttPublishChannel));
  Serial.print("): ");
  Serial.println(config.mqttPublishChannel);


  Serial.println("Reading module uuid from EEPROM");

  for (int i = 454; i < 490; ++i)
  {
    if (EEPROM.read(i) != 0) {
      config.uuid[i] = char(EEPROM.read(i));
    }
  }

  Serial.print("Uuid value (length: ");
  Serial.print(strlen(config.uuid));
  Serial.print("): ");
  Serial.println(config.uuid);
}

void setParametersToEeprom(
  String wssid, 
  String wpassw,
  String wmqttHost,
  String wmqttPort,
  String wmqttUser,
  String wmqttPass,
  String wmqttChan
) {
  clearEeprom();
  delay(10);

  int stepAddr = 0;
  
  if (wssid.length() > 1) {
    for (int i = 0; i < wssid.length(); ++i) {
      EEPROM.write(stepAddr+i, wssid[i]);
    }

    Serial.print("Write wssid : ");
    Serial.println(wssid);
    stepAddr += 32;
  }

  if (wpassw.length() > 1) {
    for (int i = 0; i < wpassw.length(); ++i) {
      EEPROM.write(stepAddr+i, wpassw[i]);
    }

    Serial.print("Write wpassw : ");
    Serial.println(wpassw);
    stepAddr += 64;
  }

  if (wmqttHost.length() > 1) {
    for (int i = 0; i < wmqttHost.length(); ++i) {
      EEPROM.write(stepAddr+i, wmqttHost[i]);
    }

    Serial.print("Write wmqttHost : ");
    Serial.println(wmqttHost);
    stepAddr += 128;
  }

  if (wmqttPort.length() > 1) {
    for (int i = 0; i < wmqttPort.length(); ++i) {
      EEPROM.write(stepAddr+i, wmqttPort[i]);
    }

    Serial.print("Write wmqttPort : ");
    Serial.println(wmqttPort);
    stepAddr += 6;
  }

  if (wmqttUser.length() > 1) {
    for (int i = 0; i < wmqttUser.length(); ++i) {
      EEPROM.write(stepAddr+i, wmqttUser[i]);
    }

    Serial.print("Write wmqttUser : ");
    Serial.println(wmqttUser);
    stepAddr += 32;
  }

  if (wmqttPass.length() > 1) {
    for (int i = 0; i < wmqttPass.length(); ++i) {
      EEPROM.write(stepAddr+i, wmqttPass[i]);
    }

    Serial.print("Write wmqttPass : ");
    Serial.println(wmqttPass);
    stepAddr += 64;
  }

  if (wmqttChan.length() > 1) {
    for (int i = 0; i < wmqttChan.length(); ++i) {
      EEPROM.write(stepAddr+i, wmqttChan[i]);
    }

    Serial.print("Write wmqttChan : ");
    Serial.println(wmqttChan);
    stepAddr += 128;
  }

  if (strlen(config.uuid) == 0) {
    String uuid = buildUuid();

    for (int i = 0; i < uuid.length(); ++i) {
      EEPROM.write(stepAddr+i, uuid[i]);
    }

    Serial.print("Write uuid : ");
    Serial.println(uuid);
  }

  EEPROM.commit();
}

bool checkEepromValues() {
  Serial.print("config.wifiSsid length : ");
  Serial.println(strlen(config.wifiSsid));

  Serial.print("config.wifiPassword length : ");
  Serial.println(strlen(config.wifiPassword));
  
  if ( strlen(config.wifiSsid) > 1 && strlen(config.wifiPassword) > 1 ) {
    return true;
  }

  Serial.println("Ssid and passw not present in EEPROM");
  return false;
}

void clearEeprom() {
  Serial.println("Clearing Eeprom");
  
  for (int i = 0; i < 512; ++i) {
    EEPROM.write(i, 0);
  }

  EEPROM.commit();
}

String buildUuid() {
  byte uuidNumber[16];
  ESP8266TrueRandom.uuid(uuidNumber);
  
  return ESP8266TrueRandom.uuidToString(uuidNumber);
}

void httpRedirect(String url) {
  server.sendHeader("Location", url, true);
  server.send(302, "text/plain", "");
}