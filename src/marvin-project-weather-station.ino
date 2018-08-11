#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <ESP8266TrueRandom.h>
#include <FS.h>
#include <ArduinoJson.h>

#define SEALEVELPRESSURE_HPA (1013.25)

struct Config {
  char wifiSsid[32];
  char wifiPassword[64];
  char mqttHost[128];
  int  mqttPort;
  char mqttUsername[32];
  char mqttPassword[64];
  char mqttPublishChannel[128];
  char mqttPublishChannelBat[128];
  char uuid[64];
};

ESP8266WebServer server(80);
WiFiClient wifiClient;
PubSubClient client(wifiClient);
Config config;
Adafruit_BME280 bme; // I2C

const char* wifiSsid        = "marvin-weather-station";
const char* wifiPassword    = "marvin-weather-station";
const String configFilePath = "/config.json";
bool wifiConnected          = false;
bool mqttConnected          = false;
const float voltMax         = 4.2;
const float voltMin         = 3.0;

void setup() {
  Serial.begin(9600);

  SPIFFS.begin();

    // Get wifi SSID and PASSW from eeprom
  if (true == getConfig()) {
    if (true == checkConfigValues()) {
      wifiConnected = wifiConnect();
  
      if (true == wifiConnected) {
        mqttConnected = mqttConnect();
      }
    }
  }

  if (false == wifiConnected) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(wifiSsid, wifiPassword);
    Serial.print("WiFi AP is ready (IP : ");  
    Serial.print(WiFi.softAPIP());
    Serial.println(")");

    server.on("/", httpParametersPage);
    server.on("/save", HTTP_POST, httpSaveParameters);
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
    ESP.deepSleep(3600e6);
    // to debug deep sleep = 10s
    // ESP.deepSleep(10e6);
  }
}

void loop() {
  if (false == wifiConnected) {
    server.handleClient();
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

void httpParametersPage() {
  /*
   * @todo gestion des erreurs
   */
  
  String response = "";
  response  = "\r\n\r\n<!DOCTYPE HTML>\r\n<html><body>";
  response += "<h1>Marvin weather station</h1>";
  response += "<h3>Wifi parameters</h3>";
  response += "<form method=\"POST\" action=\"/save\">";
  response += "<p><label style=\"min-width:90px;\" for=\"wssid\">Ssid : </label><input value=\"Bbox-94FCAFAB\" maxlength=\"32\" type=\"text\" name=\"wssid\" id=\"wssid\" placeholder=\"SSID (see on your box)\" style=\"border:1px solid #000;width:250px;\"><p>";
  response += "<p><label style=\"min-width:90px;\" for=\"wpassw\">Passord : </label><input value=\"AE55E14D3D4A2ADC1C3E1324AF224C\" maxlength=\"64\" type=\"password\" name=\"wpassw\" id=\"wpassw\" style=\"border:1px solid #000;width:250px;\"></p>";
  response += "<hr>";
  response += "<h3>Mqtt parameters</h3>";
  response += "<p><label style=\"min-width:90px;\" for=\"wmqttHost\">Host / Ip  : </label><input value=\"192.168.1.33\" maxlength=\"128\" type=\"text\" name=\"wmqttHost\" id=\"wmqttHost\" style=\"border:1px solid #000;width:250px;\"><p>";
  response += "<p><label style=\"min-width:90px;\" for=\"wmqttPort\">Port       : </label><input value=\"1883\" maxlength=\"6\" type=\"text\" name=\"wmqttPort\" id=\"wmqttPort\" style=\"border:1px solid #000;width:250px;\"></p>";
  response += "<p><label style=\"min-width:90px;\" for=\"wmqttUser\">Username   : </label><input value=\"test\" maxlength=\"32\" type=\"text\" name=\"wmqttUser\" id=\"wmqttUser\" style=\"border:1px solid #000;width:250px;\"><p>";
  response += "<p><label style=\"min-width:90px;\" for=\"wmqttPass\">Passord    : </label><input value=\"test\" maxlength=\"64\" type=\"text\" name=\"wmqttPass\" id=\"wmqttPass\" style=\"border:1px solid #000;width:250px;\"></p>";
  response += "<p><label style=\"min-width:90px;\" for=\"wmqttChanBat\">Channel Battery    : </label><input value=\"/marvin/weather-station/battery/alert\" maxlength=\"128\" type=\"text\" name=\"wmqttChanBat\" id=\"wmqttChanBat\" style=\"border:1px solid #000;width:250px;\"></p>";
  response += "<p><label style=\"min-width:90px;\" for=\"wmqttChan\">Channel    : </label><input value=\"/marvin/weather-station/value\" maxlength=\"128\" type=\"text\" name=\"wmqttChan\" id=\"wmqttChan\" style=\"border:1px solid #000;width:250px;\"></p>";
  
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
  
  ESP.reset();
}

void httpSaveParameters() {
  String response = "";
  bool error = false;

  for (int i = 0; i < server.args(); i++) {
    Serial.print(server.argName(i));
    Serial.print(" : ");
    Serial.println(server.arg(i));
  }

  if (!server.hasArg("wssid") || !server.hasArg("wpassw")){  
    error = true;
    Serial.println("No wssid and wpassw args");
  }

  if (server.arg("wssid").length() <= 1 && server.arg("wpassw").length() <= 1) {
    error = true;
    Serial.println("wssid and wpassw args is empty");
  }

  if (false == error) {
    server.arg("wssid").toCharArray(config.wifiSsid, 32);
    server.arg("wpassw").toCharArray(config.wifiPassword, 64);
    server.arg("wmqttHost").toCharArray(config.mqttHost, 128);
    config.mqttPort = server.arg("wmqttPort").toInt();
    server.arg("wmqttUser").toCharArray(config.mqttUsername, 32);
    server.arg("wmqttPass").toCharArray(config.mqttPassword, 64);
    server.arg("wmqttChanBat").toCharArray(config.mqttPublishChannelBat, 128);
    server.arg("wmqttChan").toCharArray(config.mqttPublishChannel, 128);
    
    Serial.print("wifiSsid : ");
    Serial.println(config.wifiSsid);
    Serial.print("wifiPassword : ");
    Serial.println(config.wifiPassword);
    Serial.print("mqttHost : ");
    Serial.println(config.mqttHost);
    Serial.print("mqttPort : ");
    Serial.println(config.mqttPort);
    Serial.print("mqttUsername : ");
    Serial.println(config.mqttUsername);
    Serial.print("mqttPassword : ");
    Serial.println(config.mqttPassword);
    Serial.print("mqttPublishChannelBat : ");
    Serial.println(config.mqttPublishChannelBat);
    Serial.print("mqttPublishChannel : ");
    Serial.println(config.mqttPublishChannel);

    setConfig();

    response  = "\r\n\r\n<!DOCTYPE HTML>\r\n<html><body>";
    response += "<h1>Marvin weather station</h1>";
    response += "<p>OK - Wifi parameters has been saved !</p>";
    response += "<p>OK - MQTT parameters has been saved !</p>";
    response += "<p>Please restart module to apply new parameters.</p>";
    response += "<p><a href=\"/restart\">RESTART</a></p>";
    response += "</body></html>\r\n\r\n";
    server.send(200, "text/html", response);
  } else {
    Serial.println("Redirect to /");
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
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
        if (client.connect("MarvinGroundHumidity", config.mqttUsername, config.mqttPassword)) {
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

bool getConfig() {
  File configFile = SPIFFS.open(configFilePath, "r");

  if (!configFile) {
    Serial.println("Failed to open config file \"" + configFilePath + "\".");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<512> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    Serial.println("Failed to parse config file");
    return false;
  }

  // Copy values from the JsonObject to the Config
  strlcpy(config.wifiSsid, json["wifiSsid"], sizeof(config.wifiSsid));
  strlcpy(config.wifiPassword, json["wifiPassword"], sizeof(config.wifiPassword));
  strlcpy(config.mqttHost, json["mqttHost"], sizeof(config.mqttHost));
  config.mqttPort = json["mqttPort"] | 1883;
  strlcpy(config.mqttUsername, json["mqttUsername"], sizeof(config.mqttUsername));
  strlcpy(config.mqttPassword, json["mqttPassword"], sizeof(config.mqttPassword));
  strlcpy(config.mqttPublishChannel, json["mqttPublishChannel"], sizeof(config.mqttPublishChannel));
  strlcpy(config.mqttPublishChannelBat, json["mqttPublishChannelBat"], sizeof(config.mqttPublishChannelBat));
  strlcpy(config.uuid, json["uuid"], sizeof(config.uuid));

  configFile.close();

  Serial.print("wifiSsid : ");
  Serial.println(config.wifiSsid);
  Serial.print("wifiPassword : ");
  Serial.println(config.wifiPassword);
  Serial.print("mqttHost : ");
  Serial.println(config.mqttHost);
  Serial.print("mqttPort : ");
  Serial.println(config.mqttPort);
  Serial.print("mqttUsername : ");
  Serial.println(config.mqttUsername);
  Serial.print("mqttPassword : ");
  Serial.println(config.mqttPassword);
  Serial.print("mqttPublishChannel : ");
  Serial.println(config.mqttPublishChannel);
  Serial.print("mqttPublishChannelBat : ");
  Serial.println(config.mqttPublishChannelBat);
  Serial.print("uuid : ");
  Serial.println(config.uuid);

  return true;
}

bool setConfig() {
  StaticJsonBuffer<512> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  
  json["wifiSsid"] = config.wifiSsid;
  json["wifiPassword"] = config.wifiPassword;
  json["mqttHost"] = config.mqttHost;
  json["mqttPort"] = config.mqttPort;
  json["mqttUsername"] = config.mqttUsername;
  json["mqttPassword"] = config.mqttPassword;
  json["mqttPublishChannel"] = config.mqttPublishChannel;
  json["mqttPublishChannelBat"] = config.mqttPublishChannelBat;

  if (strlen(config.uuid) == 0) {
    String tmpUuid = buildUuid();
    tmpUuid.toCharArray(config.uuid, 64);
    json["uuid"] = config.uuid;
  }

  if (SPIFFS.exists(configFilePath)) {
    SPIFFS.remove(configFilePath);
  }

  File configFile = SPIFFS.open(configFilePath, "w");
  
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  json.printTo(configFile);

  delay(100);
  getConfig();

  return true;
}

bool checkConfigValues() {
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

String buildUuid() {
  byte uuidNumber[16];
  ESP8266TrueRandom.uuid(uuidNumber);
  
  return ESP8266TrueRandom.uuidToString(uuidNumber);
}

void resetConfigFile() {
  if (SPIFFS.exists(configFilePath)) {
    SPIFFS.remove(configFilePath);
  }
}