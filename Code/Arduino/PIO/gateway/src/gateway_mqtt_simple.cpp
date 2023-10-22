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
#define SpreadingFactor 12
#define CodingRate 8
#define SyncWord 0xF3
#define PreambleLength 8
#define TxPower 20
float BAND = 868E6; // 433E6 / 868E6 / 915E6 /

bool retain = true;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String recv;
int count = 0;

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");

    // Buffer needs to be increased to accomodate the config payloads
    if(client.setBufferSize(380)){
    Serial.println("Buffer Size increased to 380 byte"); 
    }else{
     Serial.println("Failed to allocate larger buffer");   
     }

    // Create a random client ID
    String clientId = "LoRaGateway-";
    clientId += String(random(0xffff), HEX);

    // Attempt to connect
    if (client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("MQTT connected");
    } else {
      Serial.print("MQTT failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 1 seconds");
      delay(1000);
    }
  }
}

void setup() {
  Serial.begin(9600);
  while (!Serial);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  reconnect();
  // client.setCallback(MQTTCallback);

  display.init();

  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 5, "PricelessToolkit");
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
  Serial.println("Starting LoRa on " + String(BAND) + " MHz");
  if (!LoRa.begin(BAND)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }

  LoRa.setSignalBandwidth(SignalBandwidth); // signal bandwidth in Hz, defaults to 125E3
  LoRa.setSpreadingFactor(SpreadingFactor); // ranges from 6-12,default 7 see API docs
  LoRa.setCodingRate4(CodingRate);          // Supported values are between 5 and 8, these correspond to coding rates of 4/5 and 4/8. The coding rate numerator is fixed at 4.
  LoRa.setSyncWord(SyncWord);               // byte value to use as the sync word, defaults to 0x12
  LoRa.setPreambleLength(PreambleLength);   // Supported values are between 6 and 65535.
  LoRa.disableCrc();                        // Enable or disable CRC usage, by default a CRC is not used LoRa.disableCrc();
  LoRa.setTxPower(TxPower);                 // TX power in dB, defaults to 17, Supported values are 2 to 20
}

void loop() {
  if (LoRa.parsePacket()) {
    String recv = "";
    while (LoRa.available()) {
      recv += (char)LoRa.read();
    }

    Serial.println(recv);
    if (client.connected()) {
      String rs = String(LoRa.packetRssi());
      String p = "{\"code\": \"" + recv + "\",\"rssi\": " + rs + "}";
      client.publish("lora_gateway/message", String(p).c_str(), retain);
      //client.publish("LoRa-Gateway/RSSI", rs.c_str(), retain);
      client.endPublish();
    }
  }

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}

#endif