#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define SEALEVELPRESSURE_HPA    (1013.25)
#define MQTT_SERVER             "{SERVER_IP}"
#define MQTT_SERVERPORT         1883                   // use 8883 for SSL
#define MQTT_USERNAME           "{USERNAME}"
#define MQTT_PASSWORD           "{PASSWORD}"

WiFiClient wifiClient;
Adafruit_BME280 bme; // I2C
PubSubClient client(wifiClient);
const char* ssid                = "{SSID}";
const char* password            = "{PASSWORD}";

void setup() {
    Serial.begin(9600);

    WiFi.begin(ssid, password);
  
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
  
    Serial.println("");
    Serial.println("WiFi connected");  
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    if (!bme.begin(0x76)) {
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
        while (1);
    }

    bme.setSampling(Adafruit_BME280::MODE_NORMAL,
                    Adafruit_BME280::SAMPLING_X1, // temperature
                    Adafruit_BME280::SAMPLING_X1, // pressure
                    Adafruit_BME280::SAMPLING_X1, // humidity
                    Adafruit_BME280::FILTER_OFF);

    client.setServer(MQTT_SERVER, MQTT_SERVERPORT);

    while (!client.connected()) {
      Serial.print("Attempting MQTT connection...");
      // Attempt to connect
      if (client.connect("ESP8266marvin", "test", "test")) {
        Serial.println("connected");
      } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        // Wait 5 seconds before retrying
        delay(5000);
      }
    }

    client.loop();
    //client.setCallback(callback);
  
    String payload = "{\"name\":\"marvin-weather-station\", \"uuid\":\"1236584f5544fds5\", \"temperature\":{\"value\": " + String(bme.readTemperature(),2) + ", \"unit\":\"C\"},humidity:{\"value\": " + String(bme.readHumidity(),2) + ", \"unit\":\"%\"},pressure:{\"value\": " + String((bme.readPressure() / 100.0F),0) + ", \"unit\":\"hPa\"},altitude:{\"value\": " + String(bme.readAltitude(SEALEVELPRESSURE_HPA),0) + ", \"unit\":\"m\"}}";
  
    if (false == client.publish("marvin/weather/values", payload.c_str())) {
      Serial.println("Publish fail !");
    }

    // 3600e6 = 1 heure
    ESP.deepSleep(3600e6);
}

void loop() {}
