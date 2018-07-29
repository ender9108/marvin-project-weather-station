String action;

void setup() {
  Serial.begin(9600);
}

void loop() {
  action = Serial.readString();
  action.toUpperCase();

  if (action == "PING-WEATHER-STATION") {
    Serial.println(F("PONG-WEATHER-STATION"));
  }
}
