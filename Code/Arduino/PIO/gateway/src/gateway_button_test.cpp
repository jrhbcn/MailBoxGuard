#if 0

#include "secrets.h"
#include "board.h"
#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "SSD1306Wire.h"
SSD1306Wire display(OLED_ADDRESS, OLED_SDA, OLED_SCL);

///////////////////////////////////////////////////// CHANGE THIS /////////////////////////////////////////////////////////////

#define SignalBandwidth 125E3
#define SpreadingFactor 9
#define CodingRate 8
#define SyncWord 0xF3
#define PreambleLength 138
#define TxPower 20
float BAND = 868E6; // 433E6 / 868E6 / 915E6 /

bool retain = true;
String NewMailName = "mailbox";
String NewMailCode = "0xA2B2"; // For Example "0xA2B2";

String ParkingName = "parking";
String ParkingCode = "0xA2C2"; // For Example "0xA2B2";

// Assign button at GPIO04
int PIN_BUTTON = 4;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define SERIAL_DEBUG true

#if SERIAL_DEBUG 
  #define BEGIN_DEBUG() do { Serial.begin (9600); } while (0)
  #define TRACE(x)       Serial.print(x)
  #define TRACELN(x)     Serial.println(x)
#else
  #define BEGIN_DEBUG()  ((void) 0)
  #define TRACE(x)       ((void) 0)
  #define TRACELN(x)     ((void) 0)
#endif // SERIAL_DEBUG

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String recv;

String bat_val = "";
String received_code = "";

unsigned long previous_millis = 0;
unsigned long seconds = 0;

bool waiting_for_ack = false;
unsigned long seconds_ack = ULONG_MAX;



void setup() {

  BEGIN_DEBUG();

  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 5, "LoRa");
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 20, "TEST Button");
  display.drawString(64, 38, "RX " + String(BAND));
  display.display();

  delay(2000);

  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 5, "WIFI CONNECTED");
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 20, "LoRa Gateway");
  display.drawString(64, 38, "RX " + String(BAND));
  display.display();

  delay(2000);
  display.clear();
  display.display();

  SPI.begin(CONFIG_CLK, CONFIG_MISO, CONFIG_MOSI, CONFIG_NSS);
  LoRa.setPins(CONFIG_NSS, CONFIG_RST, CONFIG_DIO0);
  TRACELN("Starting LoRa on " + String(BAND) + " MHz");
  delay(1000);
  if (!LoRa.begin(BAND)) {
    TRACELN("Starting LoRa failed!");
    ESP.restart(); // while (1);
  }

  LoRa.setSignalBandwidth(SignalBandwidth); // signal bandwidth in Hz, defaults to 125E3
  LoRa.setSpreadingFactor(SpreadingFactor); // ranges from 6-12,default 7 see API docs
  LoRa.setCodingRate4(CodingRate);          // Supported values are between 5 and 8, these correspond to coding rates of 4/5 and 4/8. The coding rate numerator is fixed at 4.
  LoRa.setSyncWord(SyncWord);               // byte value to use as the sync word, defaults to 0x12
  LoRa.setPreambleLength(PreambleLength);   // Supported values are between 6 and 65535.
  LoRa.enableCrc();                        // Enable or disable CRC usage, by default a CRC is not used LoRa.disableCrc();
  LoRa.setTxPower(TxPower);                 // TX power in dB, defaults to 17, Supported values are 2 to 20

  // Setup pin for button
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  
}

void parsePacket(String rawpkg){
  int i = 0;
  int intconv;

  // Empty vals to be sure we have new data
  bat_val = "";
  received_code = "";
  
  while(rawpkg[i] != ',' && rawpkg[i] != '\0' ){
    i++;
  }

  received_code=rawpkg.substring(0,i);

  // Catch if Battery Percentage is not sent
  if(rawpkg.length() <= i+1){
    bat_val = "UNDEFINED";
   } else {
    // "Converts" float to int - For cosmetic reasons. Modify, if desired
    bat_val=rawpkg.substring(i+1);
    intconv = bat_val.toFloat() + 0;
    bat_val = String(intconv);
   }
  
}

void send_lora_parking()
{
  TRACELN("Button pressed! starting to send LoRa message");
  // Send LoRa message to sensor
  int loopcounter = 0;
  LoRa.enableInvertIQ();
  if (loopcounter < 2) 
  {
    TRACE("Sending message ... ");
    delay (10);
    LoRa.beginPacket();
    LoRa.print(ParkingCode);
    LoRa.endPacket();
    delay (10);
    loopcounter++;
    TRACELN("OK.");
  }
  LoRa.disableInvertIQ();

  // Wait for ACK from sensor in loop()
  waiting_for_ack = true;
  seconds_ack = seconds;

}


void loop() {

  unsigned long current_millis = millis();

  // Increase the number of seconds
  if (current_millis - previous_millis >= 1000) {
    seconds += (current_millis - previous_millis)/1000;
    previous_millis = current_millis;
    TRACE("Seconds = "); TRACELN(seconds);
  }

  // Check if button is pressed
  if ( (digitalRead(PIN_BUTTON) == LOW)&&(!waiting_for_ack) ) {
    send_lora_parking();
  }

  if (waiting_for_ack)
  {
    if (seconds > seconds_ack + 10 ) 
    {
      TRACELN("Waiting for ACK for more than 10 seconds!!!!");
      waiting_for_ack = false;
      seconds_ack = ULONG_MAX;
    }
  }

  if (LoRa.parsePacket())
  {
    String recv = "";
    while (LoRa.available()) {
      recv += (char)LoRa.read();
    }

    TRACE("LoRa message received: "); TRACELN(recv);

    // Parse message
    parsePacket(recv);

    String rs = String(LoRa.packetRssi());
    String p = "{\"code\": \"" + recv + "\",\"rssi\": " + rs + "}";

    if(received_code == ParkingCode) 
    {
      TRACE("ParkingCode received: "); TRACELN(p.c_str());
      waiting_for_ack = false;
      seconds_ack = ULONG_MAX;
    }

  }

}

#endif