/*
 * Berryfrop_tx by teglo (c)2016
 * 
 * Program to get weather datas with sensors  
 * Atmel ATmeag328P, Microcontroller, 8Bit, 16MHz, 32KB Flash
 * DHT11/22
 * BMP180
 */

//Power management
#include "LowPower.h"

//DHT sensor
#include "DHT.h"
#define DHTPIN 12
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

int humidity_dht;
int temp_dht;
int temp_dht_c;
int temp_dht_f;

int adc_low, adc_high;
long adc_result;
long vcc;

//RF transmitter
#include <RCSwitch.h>
RCSwitch mySwitch = RCSwitch();

//BMP180 sensor
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>
#include "Adafruit_SI1145.h"
Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);
Adafruit_SI1145 uv = Adafruit_SI1145();
 
int i = 0; //loop counter
int ri = 0; //reset counter
int li = 1; //loop counter
//int interval = 74;
int interval = 1;

int warnled = 13;
 
void setup() {
  
  Serial.begin(9600);

  //Initialise the sensors
  dht.begin();
  if(!bmp.begin()){
    Serial.print("No BMP085/180 detected ... Check your wiring or I2C ADDR!");
    while(1);
  }
  
  //Initialise the transmitter unit
  mySwitch.enableTransmit(10);
  mySwitch.setPulseLength(340);
  mySwitch.setProtocol(2);
  mySwitch.setRepeatTransmit(10);

  // initialize digital pin 13 as an output.
  pinMode(warnled, OUTPUT);

  //Referenzvoltage for ADC
  ADMUX |= (1<<REFS0);
  //1.1V as input for ADC
  ADMUX |= (1<<MUX3) | (1<<MUX2) | (1<<MUX1);
  delay(10);
  ADCSRA |= (1<<ADEN);

  Serial.println("Adafruit SI1145 test");
  
  if (! uv.begin()) {
    Serial.println("Didn't find Si1145");
    while (1);
  }

  Serial.println("SI1145 is OK!");

}

//Declare reset function at address 0
void(* resetFunc) (void) = 0; 

void loop() {

    //Reset controller once a day
    if (ri >= 3600000) {
      resetFunc();
    }
    
    if (i >= interval || i == 0) {

      /*
       * Send values
       * #   first numbers is transmitter ID
       * ##    Secound two numbers are the value of measure type
       * ####  Last four are the measure value of measure type
       * 
       * #11#### = DHT Temperature in Celsius, Unit: °C
       * #12#### = DHT Temperature in Fahrenheit, Unit: °F
       * #13#### = DHT Humidity in Percent, Unit: %
       * #14#### = BMP Pressure in Pascal, Unit: hPa
       * #15#### = BMP Altitude in Meter, Unit: m
       * #16#### = BMP Temperatur in Celsius, Unit: °C
       * #17#### = ATMega Voltage in Millivolt, Unit: mV
       * 
       */
      
      i = 0;

      //mySwitch.send( 10000000 + li, 24);
      mySwitch.send( 1980, 24);
      
      float humidity_dht = dht.readHumidity();
      float temp_dht_c = dht.readTemperature();
      float temp_dht_hic = dht.computeHeatIndex(temp_dht_c, humidity_dht, false);
      float temp_dht_f = dht.readTemperature(true);
      float temp_dht_hif = dht.computeHeatIndex(temp_dht_f, humidity_dht);

      if(temp_dht_c) {
        if(temp_dht_c >= 100) {
          mySwitch.send( 1110000 + (temp_dht_hic *10), 24);
            Serial.print("DHT Temp:    ");
            Serial.print(temp_dht_hic);
            Serial.println("C");
        } else {
          mySwitch.send( 11100000 + (temp_dht_hic *100), 24);
            Serial.print("DHT Temp:    ");
            Serial.print(temp_dht_hic);
            Serial.println("C");
        }
      }
      if(temp_dht_f) {
        if(temp_dht_c >= 100) {
          mySwitch.send( 11200000 + (temp_dht_hif *10), 24);
            Serial.print("DHT Temp:    ");
            Serial.print(temp_dht_hif);
            Serial.println("F");
        } else {
          mySwitch.send( 11200000 + (temp_dht_hif *100), 24);
            Serial.print("DHT Temp:    ");
            Serial.print(temp_dht_hif);
            Serial.println("F");
        }
      }
      if(humidity_dht) {
        if(humidity_dht >= 100) {
          mySwitch.send( 11300000 + (humidity_dht *10), 24);
            Serial.print("Humidity:    ");
            Serial.print(humidity_dht);
            Serial.println("%");
        } else {
          mySwitch.send( 11300000 + (humidity_dht *100), 24);
            Serial.print("Humidity:    ");
            Serial.print(humidity_dht);
            Serial.println("%");
        }
      }
      /* Get a new sensor event */ 
      sensors_event_t event;
      bmp.getEvent(&event);

      if (event.pressure) {     
        mySwitch.send( 11400000 + (event.pressure *10),24);
          Serial.print("Pressure:    ");
          Serial.print(event.pressure);
          Serial.println("hPa");
     
        float seaLevelPressure = SENSORS_PRESSURE_SEALEVELHPA;
        mySwitch.send( 11500000 + (bmp.pressureToAltitude(seaLevelPressure,event.pressure) *10),24);
          Serial.print("Altitude:    "); 
          Serial.print(bmp.pressureToAltitude(seaLevelPressure,event.pressure));
          Serial.println("m");

        float temp_bmp;
        bmp.getTemperature(&temp_bmp);
 
        if (temp_bmp) {
          mySwitch.send( 11600000 + (temp_bmp *100),24);
            Serial.print("Temperature: ");
            Serial.print(temp_bmp);
            Serial.println("Celsius");
        }
      }

      //Get Battery status
      ADCSRA |= (1<<ADSC);

      while (bitRead(ADCSRA, ADSC));
      adc_low = ADCL;
      adc_high = ADCH;

      adc_result = (adc_high<<8) | adc_low;

      //Result voltage measurement
      vcc = 1125300L / adc_result;

      mySwitch.send( 11700000 + vcc,24);

      if (vcc < 3200) {
        digitalWrite(warnled, HIGH);
      } else {
        digitalWrite(warnled, LOW);
      }

      //send data of SI1145 module
      mySwitch.send( 11800000 + uv.readVisible(),24);
      Serial.print("Vis: "); Serial.println(uv.readVisible());

      float UVindex = uv.readUV();
      UVindex /= 100.0;
      mySwitch.send( 11900000 + UVindex *1000,24);
      Serial.print("UV Index: ");  Serial.println(UVindex);
      
      //Done with all values, sending ends here
     mySwitch.send( 1990, 24);

      li++; //increment counter for loop
    }

      //Put controller in sleep modus for 8sec.
      LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
      
      ri++; //increment counter for reset
      i++;
}
