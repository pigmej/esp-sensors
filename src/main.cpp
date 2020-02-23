#include "MHZ19.h"
#include "secure_settings.h" // secure...
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <DHT.h>
#include <DHT_U.h>
#include <MQTT.h>
#include <WiFi.h>

#define DHTPIN 4
#define DHTTYPE DHT22 // DHT 22 (AM2302)

#define RX_PIN 17 // Rx pin which the MHZ19 Tx pin is attached to
#define TX_PIN 16 // Tx pin which the MHZ19 Rx pin is attached to
#define BAUDRATE                                                               \
  9600 // Device to MH-Z19 Serial baudrate (should not be changed)

DHT_Unified dht(DHTPIN, DHTTYPE);

// uint32_t delayMS;

MHZ19 myMHZ19; // Constructor for library

HardwareSerial mySerial(1); // (ESP32 Example) create device to MH-Z19 serial

WiFiClient net;
MQTTClient mqtt_client;

unsigned long getDataTimer = 0;

void MQTT_Connect() {
  int i = 0;
  Serial.print("checking wifi status");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
    i++;
    if (i > 60) {
      return;
    }
  }
  Serial.print("\nConnecting to MQTT");
  while (!mqtt_client.connect(mqtt_clientId, mqtt_user, mqtt_password)) {
    Serial.print(",");
    delay(1000);
  }

  Serial.println("\nconnected!");
  mqtt_client.subscribe("/ping");
}

void messageReceived(String &topic, String &payload) {
  Serial.println("incoming: " + topic + " - " + payload);

  // Note: Do not use the client in the callback to publish, subscribe or
  // unsubscribe as it may cause deadlocks when other things arrive while
  // sending and receiving acknowledgments. Instead, change a global variable,
  // or push to a queue and handle it in the loop after calling `client.loop()`.
}

void setup() {
  Serial.begin(115200); // Device to serial monitor feedback

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.printf("WiFi Failed!\n");
    return;
  }

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  mySerial.begin(BAUDRATE, SERIAL_8N1, RX_PIN,
                 TX_PIN); // (ESP32 Example) device to MH-Z19 serial start
  myMHZ19.begin(
      mySerial); // *Serial(Stream) refence must be passed to library begin().
  myMHZ19.setFilter(true, false);
  myMHZ19.autoCalibration(); // Turn auto calibration ON (OFF
                             // autoCalibration(false))

  dht.begin();
  // Print temperature sensor details.
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);

  mqtt_client.begin(mqtt_host, net);
  mqtt_client.onMessage(messageReceived);
  MQTT_Connect();
}

void loopCO2() {
  int CO2;

  /* note: getCO2() default is command "CO2 Unlimited". This returns the correct
  CO2 reading even if below background CO2 levels or above range (useful to
  validate sensor). You can use the
  usual documented command with getCO2(false) */

  CO2 = myMHZ19.getCO2(true, true); // Request CO2 (as ppm)

  byte thisCode = myMHZ19.errorCode;

  // handle code based upon error type
  if (thisCode != RESULT_OK) {
    if (thisCode == RESULT_FILTER) {
      Serial.println("*** Filter was triggered ***");
      Serial.print("Offending Value: ");
      Serial.println(CO2);
    } else {
      Serial.print("Communication Error Found. Error Code: ");
      Serial.println(thisCode);
    }
  }
  // error code was result OK. Print as "normal"
  else {
    Serial.print("CO2: ");
    Serial.print(CO2);
    Serial.println(" PPM");
    mqtt_client.publish("/home/" + String(mqtt_clientId) + "/co2", String(CO2));
  }
}

void loopTemp() {
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  if (isnan(event.temperature)) {
    Serial.println(F("Error reading temperature!"));
  } else {
    Serial.print(F("Temperature: "));
    Serial.print(event.temperature);
    Serial.println(F("°C"));
    mqtt_client.publish("/home/" + String(mqtt_clientId) + "/temperature",
                        String(event.temperature));
  }
  // Get humidity event and print its value.
  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity)) {
    Serial.println(F("Error reading humidity!"));
  } else {
    Serial.print(F("Humidity: "));
    Serial.print(event.relative_humidity);
    Serial.println(F("%"));
    mqtt_client.publish("/home/" + String(mqtt_clientId) + "/humidity",
                        String(event.relative_humidity));
  }
}

void loop() {
  mqtt_client.loop();
  if (!mqtt_client.connected()) {
    MQTT_Connect();
  }

  if (millis() - getDataTimer >= 2000) {
    loopCO2();
    loopTemp();
    getDataTimer = millis();
  }
}