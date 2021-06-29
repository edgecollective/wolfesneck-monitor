/*
  TimedWakeup

  This sketch demonstrates the usage of Internal Interrupts to wakeup a chip in sleep mode.
  Sleep modes allow a significant drop in the power usage of a board while it does nothing waiting for an event to happen. Battery powered application can take advantage of these modes to enhance battery life significantly.

  In this sketch, the internal RTC will wake up the processor every 2 seconds.
  Please note that, if the processor is sleeping, a new sketch can't be uploaded. To overcome this, manually reset the board (usually with a single or double tap to the RESET button)

  This example code is in the public domain.
*/
#include <RHSoftwareSPI.h> // http://www.airspayce.com/mikem/arduino/RadioHead/RadioHead-1.113.zip
#include <RHRouter.h>
#include <RHMesh.h>
#include <RH_RF95.h>
#define RH_HAVE_SERIAL
#include <SPI.h>
#include <Wire.h>
#include "ArduinoLowPower.h"
#include <Adafruit_SleepyDog.h>
#include <Adafruit_AM2315.h> //https://learn.adafruit.com/am2315-encased-i2c-temperature-humidity-sensor/arduino-code
#include <ArduinoJson.h> //https://arduinojson.org/v6/doc/installation/

// Class to manage message delivery and receipt, using the driver declared above
RHMesh *manager;

#include "config.h"

#define VBATPIN A7

int wake_counter = 0;
// note: total sleep time is sleep_interval*wake_counter_max

// for feather m0  
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3


// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 915.0

#define gatewayNode 1

// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

Adafruit_AM2315 am2315;

#define sensorID 22

int sensor_working = 0;  // flag for whether sensor was working
int max_sensor_attempts = 10;

typedef struct {
  int co2;
  float temperature;
  float humidity;
  float battery;
  char loranet_pubkey[13]; //pubkey for this lora network (same as bayou pubkey of node #1 / the gateway node)
  int node_id; 
  int next_hop;
  int next_rssi;
  int logcode;
} Payload;

Payload theData;


void setup() {

// startup (or watchdog reset)
  for (int j=0;j<10;j++) {
    digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  }
  
  int countdownMS = Watchdog.enable(watchdog_interval); // max 16 seconds, try 10

  Serial.print("Watchdog enabled! interval (ms): ");
  Serial.println(watchdog_interval);
  
  Serial.begin(9600);

  /*
  while(!Serial) {
    ;
  }
  */

    Serial.println("-----Starting up!----\n");

  sensor_working = am2315.begin();

  if(!sensor_working) {
    Serial.println("sensor not working -- check wiring?");
  }

  while (!sensor_working) {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(50);
  digitalWrite(LED_BUILTIN, LOW);
  delay(50);
  }
 
  Serial.print("sensor works!");
  
  pinMode(LED_BUILTIN, OUTPUT);
  // Uncomment this function if you wish to attach function dummy when RTC wakes up the chip
  // LowPower.attachInterruptWakeup(RTC_ALARM_WAKEUP, dummy, CHANGE);

  manager = new RHMesh(rf95, this_node_id);

  if (!manager->init()) {
    Serial.println(F("mesh init failed"));
    
  } else {
    delay(1000);
  }
  rf95.setTxPower(23, false);
  rf95.setFrequency(915.0);
  rf95.setCADTimeout(500);

  // Possible configurations:
  // Bw125Cr45Sf128 (the chip default)
  // Bw500Cr45Sf128
  // Bw31_25Cr48Sf512
  // Bw125Cr48Sf4096

  // long range configuration requires for on-air time
  boolean longRange = false;
  if (longRange) {
    RH_RF95::ModemConfig modem_config = {
      0x78, // Reg 0x1D: BW=125kHz, Coding=4/8, Header=explicit
      0xC4, // Reg 0x1E: Spread=4096chips/symbol, CRC=enable
      0x08  // Reg 0x26: LowDataRate=On, Agc=Off.  0x0C is LowDataRate=ON, ACG=ON
    };
    rf95.setModemRegisters(&modem_config);
    if (!rf95.setModemConfig(RH_RF95::Bw125Cr48Sf4096)) {
      Serial.println(F("set config failed"));
    }
  }
  
  rf95.sleep();

  
}

void loop() {
  Watchdog.reset();

  if (wake_counter==0) { // then read the sensor and send
    
float temperature, humidity;
int sensor_read_status=0;

  delay(2000);
  sensor_read_status=am2315.readTemperatureAndHumidity(&temperature, &humidity);

    while (!sensor_read_status) {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(50);
  digitalWrite(LED_BUILTIN, LOW);
  delay(50);
  }

digitalWrite(LED_BUILTIN, LOW);
    // send packet via radio
    Serial.println("sensor worked!");

   float measuredvbat = analogRead(VBATPIN);
measuredvbat *= 2;    // we divided by 2, so multiply back
measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
measuredvbat /= 1024; // convert to voltage
Serial.print("VBat: " ); Serial.println(measuredvbat);



  //itoa(packetnum++, radiopacket+13, 10);
//  Serial.print("Sending "); Serial.print(radiopacket); Serial.println(" ...");
  delay(10);
  


  int logcode = 1;
  
  //theData.co2 = co2;
  theData.temperature = temperature;
  theData.humidity = humidity;
  theData.battery = measuredvbat;
  memcpy(theData.loranet_pubkey,loranet_pubkey,13);
  theData.node_id = this_node_id;
  theData.next_rssi = rf95.lastRssi();
  theData.logcode = logcode;
  RHRouter::RoutingTableEntry *route = manager->getRouteTo(gatewayNode);
  if (route != NULL) {
  theData.next_hop = route->next_hop;
  }
  else
  {
  theData.next_hop = 0; // consider this an 'error' message of sorts
  }
  
  // send an acknowledged message to the target node
    uint8_t error = manager->sendtoWait((uint8_t *)&theData, sizeof(theData), gatewayNode);
    if (error != RH_ROUTER_ERROR_NONE) {
      Serial.println();
      Serial.print(F(" ! "));
      Serial.println(getErrorString(error));

          
    } else {

          //u8x8.setFont(u8x8_font_chroma48medium8_r);
          //u8x8.setCursor(9,1);
          //u8x8.print("        ");
          if (theData.next_hop==0) {
            Serial.println("routing ...");
          }
          else {
            Serial.print("hop:");
            Serial.println(theData.next_hop);
            Serial.print("rssi:");
            Serial.println(theData.next_rssi);
            Serial.println(F("OK"));
          }
          
    }

  
  rf95.sleep();



  

  for (int j=0;j<2;j++) {
    digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  }
  
  }

  wake_counter=wake_counter+1;
  if (wake_counter>=wake_counter_max) {
    wake_counter=0;
  }

  
  digitalWrite(LED_BUILTIN, HIGH);
  delay(10);
  digitalWrite(LED_BUILTIN, LOW);
  delay(10);

  Serial.println("sleep chunk ...");
   LowPower.sleep(sleep_interval);
   //delay(sleep_interval);
   
  rf95.sleep();
}

const __FlashStringHelper* getErrorString(uint8_t error) {
  switch(error) {
    case 1: return F("invalid length");
    break;
    case 2: return F("no route");
    break;
    case 3: return F("timeout");
    break;
    case 4: return F("no reply");
    break;
    case 5: return F("unable to deliver");
    break;
  }
  return F("unknown");
}
