#include <Arduino.h>
#include "MHZ19.h"
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#define DHTPIN 4
#define DHTTYPE    DHT22     // DHT 22 (AM2302)

#define RX_PIN 17                                          // Rx pin which the MHZ19 Tx pin is attached to
#define TX_PIN 16                                          // Tx pin which the MHZ19 Rx pin is attached to
#define BAUDRATE 9600                                      // Device to MH-Z19 Serial baudrate (should not be changed)

DHT_Unified dht(DHTPIN, DHTTYPE);

//uint32_t delayMS;

MHZ19 myMHZ19;                                             // Constructor for library

HardwareSerial mySerial(1);                              // (ESP32 Example) create device to MH-Z19 serial

unsigned long getDataTimer = 0;

void setup()
{
    Serial.begin(115200);                                     // Device to serial monitor feedback
 
    mySerial.begin(BAUDRATE, SERIAL_8N1, RX_PIN, TX_PIN); // (ESP32 Example) device to MH-Z19 serial start   
    myMHZ19.begin(mySerial);                                // *Serial(Stream) refence must be passed to library begin(). 
    myMHZ19.setFilter(true, false);
    myMHZ19.autoCalibration();                              // Turn auto calibration ON (OFF autoCalibration(false))


    dht.begin();
    // Print temperature sensor details.
    sensor_t sensor;
    dht.temperature().getSensor(&sensor);
}

void loopCO2()
{
  int CO2; 

  /* note: getCO2() default is command "CO2 Unlimited". This returns the correct CO2 reading even 
  if below background CO2 levels or above range (useful to validate sensor). You can use the 
  usual documented command with getCO2(false) */

  CO2 = myMHZ19.getCO2(true, true);                             // Request CO2 (as ppm)                             

  byte thisCode = myMHZ19.errorCode;

  // handle code based upon error type
  if(thisCode != RESULT_OK)
  {
      if(thisCode == RESULT_FILTER)
      {
          Serial.println("*** Filter was triggered ***");
          Serial.print("Offending Value: ");
          Serial.println(CO2);
      }
      else
      {
          Serial.print("Communication Error Found. Error Code: ");
          Serial.println(thisCode);
      }
  } 
  // error code was result OK. Print as "normal" 
  else
  {
      Serial.print("CO2: ");
      Serial.print(CO2);
      Serial.println(" PPM");
  }                               
}

void loopTemp()
{
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  if (isnan(event.temperature)) {
    Serial.println(F("Error reading temperature!"));
  }
  else {
    Serial.print(F("Temperature: "));
    Serial.print(event.temperature);
    Serial.println(F("°C"));
  }
  // Get humidity event and print its value.
  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity)) {
    Serial.println(F("Error reading humidity!"));
  }
  else {
    Serial.print(F("Humidity: "));
    Serial.print(event.relative_humidity);
    Serial.println(F("%"));
  }
}

void loop()
{
    if (millis() - getDataTimer >= 2000)
    {
      loopCO2();
      loopTemp();
      getDataTimer = millis();
    }
    
}