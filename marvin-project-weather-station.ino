#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define SEALEVELPRESSURE_HPA    (1013.25)

struct Config {
  char* wifiSsid              = "{{ wifi_ssid }}";
  char* wifiPassword          = "{{ wifi_password }}";
  char* mqttIp                = "{{ mqtt_ip }}";
  int   mqttPort              = 1883;
  char* mqttUsername          = "{{ mqtt_username }}";
  char* mqttPassword          = "{{ mqtt_password }}";
  char* mqttPublishChannel    = "{{ mqtt_publish_channel }}";
  char* mqttPublishChannelBat = "{{ mqtt_publish_channel_battery }}";
  char* uuid                  = "{{ module_uuid }}";
};

Config config;
WiFiClient wifiClient;
Adafruit_BME280 bme; // I2C
PubSubClient client(wifiClient);
const float voltMax           = 4.2;
const float voltMin           = 3.0;

void setup() {
    Serial.begin(9600);

    wifiConnect();

    if (!bme.begin(0x76)) {
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
        while (1);
    }

    bme.setSampling(Adafruit_BME280::MODE_NORMAL,
                    Adafruit_BME280::SAMPLING_X1, // temperature
                    Adafruit_BME280::SAMPLING_X1, // pressure
                    Adafruit_BME280::SAMPLING_X1, // humidity
                    Adafruit_BME280::FILTER_OFF);

    mqttConnect();
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

void loop() {}

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

void wifiConnect() {
    WiFi.begin(config.wifiSsid, config.wifiPassword);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
  
    Serial.println("");
    Serial.println("WiFi connected");  
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void mqttConnect() {
    client.setServer(config.mqttIp, config.mqttPort);

    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        if (client.connect("ESP8266", config.mqttUsername, config.mqttPassword)) {
            Serial.println("connected");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}
