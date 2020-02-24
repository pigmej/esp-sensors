#include <MHZ19.h>
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <DHT.h>
#include <DHT_U.h>
#include <MQTT.h>
#include <WiFi.h>
#include <ArduinoOTA.h>

#include "secure_settings.h" // secure...

#define DHTPIN 4
#define DHTTYPE DHT22

#define RX_PIN 17
#define TX_PIN 16
#define BAUDRATE 9600


DHT_Unified dht(DHTPIN, DHTTYPE);

MHZ19 myMHZ19;

HardwareSerial mySerial(1);

WiFiClient net;
MQTTClient mqtt_client;

unsigned long getDataTimer = 0;

const String _mqtt_name = String(MQTT_CLIENTID);

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
  while (!mqtt_client.connect(MQTT_CLIENTID, mqtt_user, mqtt_password)) {
    Serial.print(",");
    delay(1000);
  }

  Serial.println("\nconnected!");
  mqtt_client.subscribe("/ping");
}

void messageReceived(String &topic, String &payload) {
  Serial.println("incoming mqtt: " + topic + " - " + payload);

  // Note: Do not use the client in the callback to publish, subscribe or
  // unsubscribe as it may cause deadlocks when other things arrive while
  // sending and receiving acknowledgments. Instead, change a global variable,
  // or push to a queue and handle it in the loop after calling `client.loop()`.
}

void setup() {
  Serial.begin(115200);

  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(DEVICE_NAME);
  WiFi.setAutoReconnect(true);
  WiFi.begin(wifi_ssid, wifi_password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.printf("WiFi Failed!\n");
    return;
  }

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  mySerial.begin(BAUDRATE, SERIAL_8N1, RX_PIN, TX_PIN);
  myMHZ19.begin(mySerial);
  myMHZ19.setFilter(true, false);
  myMHZ19.autoCalibration();
  dht.begin();

  sensor_t sensor;
  dht.temperature().getSensor(&sensor);

  mqtt_client.begin(mqtt_host, net);
  mqtt_client.onMessage(messageReceived);
  MQTT_Connect();

  ArduinoOTA.setPort(3232);
  ArduinoOTA.setHostname(DEVICE_NAME);
  ArduinoOTA.setPassword(ota_password);


  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();
  
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
    mqtt_client.publish("/home/" + _mqtt_name + "/co2", String(CO2));
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
    mqtt_client.publish("/home/" + _mqtt_name + "/temperature",
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
    mqtt_client.publish("/home/" + _mqtt_name + "/humidity",
                        String(event.relative_humidity));
  }
}

void loop() {
  mqtt_client.loop();
  ArduinoOTA.handle();
  if (!mqtt_client.connected()) {
    MQTT_Connect();
  }

  if (millis() - getDataTimer >= 30 * 1000) {
    loopCO2();
    loopTemp();
    getDataTimer = millis();
  }
}
