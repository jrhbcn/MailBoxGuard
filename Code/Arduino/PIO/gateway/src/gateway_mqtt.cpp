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
String NewMailName = "mailbox";
String NewMailCode = "0xA2B2"; // For Example "0xA2B2";

String bat_val = "";
String received_code = "";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define SERIAL_DEBUG true
#define MQTT_DEBUG true

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
int count = 0;

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  delay(10);
  TRACE("Connecting to "); TRACELN(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    TRACE(".");
  }

  TRACELN(""); TRACELN("WiFi connected");
  TRACE("IP address: "); TRACELN(WiFi.localIP());
}

void reconnect() {
  int retries = 0;
  while (!client.connected()) {
    TRACE("Attempting MQTT connection...");
    // Buffer needs to be increased to accomodate the config payloads
    if(client.setBufferSize(380)){
    TRACELN("Buffer Size increased to 380 byte"); 
    }else{
     TRACELN("Failed to allocate larger buffer");   
     }

    // Create a random client ID
    String clientId = "LoRaGateway-";
    clientId += String(random(0xffff), HEX);

    // Attempt to connect
    if (client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      TRACELN("MQTT connected"); 
      // Send auto-discovery message for sensor
      String t,m; 

      // LORA GATEWAY
      String DEVICE_ID = "lora_gateway";
      String DEVICE_NAME = "LoRa Gateway";

      // RSSI
      t = "homeassistant/sensor/" + DEVICE_ID + "/rssi/config";
      m = "{\"device\":{\"ids\":[\"" + DEVICE_ID + "\"],\"name\":\"" + DEVICE_NAME + "\",\"manufacturer\":\"TTGO\"}," +
          "\"name\": \"RSSI\"," + 
          "\"device_class\": \"signal_strength\"," + 
          "\"unit_of_measurement\": \"dBm\"," + 
          "\"entity_category\": \"diagnostic\"," + 
          "\"unique_id\": \"" + DEVICE_ID + "_rssi\"," + 
          "\"state_topic\": \"lora_gateway/gateway/rssi\","
          "\"force_update\": true," + 
          "\"expire_after\": 90400," + 
          "\"retain\": true}";
      client.publish(t.c_str(), m.c_str(), retain);

      // IP
      t = "homeassistant/sensor/" + DEVICE_ID + "/ip/config";
      m = "{\"device\":{\"ids\":[\"" + DEVICE_ID + "\"],\"name\":\"" + DEVICE_NAME + "\",\"manufacturer\":\"TTGO\"}," +
          "\"name\": \"IP\"," + 
          "\"entity_category\": \"diagnostic\"," + 
          "\"unique_id\": \"" + DEVICE_ID + "_ip\"," + 
          "\"state_topic\": \"lora_gateway/gateway/ip\","
          "\"retain\": true}";
      client.publish(t.c_str(), m.c_str(), true);

      // RAW
      t = "homeassistant/sensor/" + DEVICE_ID + "/raw/config";
      m = "{\"device\":{\"ids\":[\"" + DEVICE_ID + "\"],\"name\":\"" + DEVICE_NAME + "\",\"manufacturer\":\"TTGO\"}," +
          "\"name\": \"Raw\"," + 
          "\"entity_category\": \"diagnostic\"," + 
          "\"unique_id\": \"" + DEVICE_ID + "_raw\"," + 
          "\"state_topic\": \"lora_gateway/gateway/raw\"";
      client.publish(t.c_str(), m.c_str(), true);

      // MAILBOX
      t = "homeassistant/switch/mailbox/config";
      m = "{\"device\":{\"ids\":[\"mailbox\"],\"name\":\"Mailbox\",\"mf\":\"PricelessToolkit\"},"
          "\"name\": \"Mail\","
          "\"icon\": \"mdi:mailbox\","
          "\"device_class\": \"switch\","
          "\"unique_id\": \"mailbox_sensor\","
          "\"state_topic\": \"lora_gateway/mailbox/state\","
          "\"command_topic\": \"lora_gateway/mailbox/state\","
          "\"retain\":true}";
      client.publish(t.c_str(), m.c_str(), retain);

      t = "homeassistant/sensor/mailbox/rssi/config";
      m = "{\"device\":{\"ids\":[\"mailbox\"],\"name\":\"Mailbox\",\"mf\":\"PricelessToolkit\"},"
          "\"name\": \"RSSI\","
          "\"device_class\": \"signal_strength\","
          "\"unit_of_measurement\": \"dBm\","
          "\"entity_category\": \"diagnostic\","
          "\"unique_id\": \"mailbox_rssi\","
          "\"state_topic\": \"lora_gateway/mailbox/rssi\","
          "\"retain\":true}";
      client.publish(t.c_str(), m.c_str(), retain);

      t = "homeassistant/sensor/mailbox/battery/config";
      m = "{\"device\":{\"ids\":[\"mailbox\"],\"name\":\"Mailbox\",\"mf\":\"PricelessToolkit\"},"
          "\"name\": \"Battery\","
          "\"device_class\": \"battery\","
          "\"unit_of_measurement\": \"%\","
          "\"entity_category\": \"diagnostic\","
          "\"unique_id\": \"mailbox_battery\","
          "\"state_topic\": \"lora_gateway/mailbox/battery\","
          "\"retain\":true}";
      client.publish(t.c_str(), m.c_str(), retain);



    } else {
      TRACE("MQTT failed, rc="); TRACELN(client.state());
      retries++;
      TRACELN(" try again in 3 seconds");
      delay(3000);
    }

    if (retries > 10) {
      display.clear();
      display.setFont(ArialMT_Plain_16);
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.drawString(64, 20, "RESTARTING");
      display.display();
      delay(2000);
      ESP.restart();
    }

  }
}

void mqtt_lora_gateway_update() {
  String topic = "lora_gateway/gateway";
  client.publish((topic + "/ip").c_str(), WiFi.localIP().toString().c_str(), retain);
  client.publish((topic + "/rssi").c_str(), String(WiFi.RSSI()).c_str(), retain);
}


void setup() {

  BEGIN_DEBUG();

  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 5, "STARTING WIFI");
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 20, "LoRa Gateway");
  display.drawString(64, 38, "RX " + String(BAND));
  display.display();


  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  reconnect();
  // client.setCallback(MQTTCallback);

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

  mqtt_lora_gateway_update();
  client.endPublish();
  
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

  //Catch if Battery Percentage is not sent
  if(rawpkg.length() <= i+1){
    bat_val = "UNDEFINED";
   } else {
    // "Converts" float to int - For cosmetic reasons. Modify, if desired
    bat_val=rawpkg.substring(i+1);
    intconv = bat_val.toFloat() + 0;
    bat_val = String(intconv);
   }
  
}

void loop() {
  if (LoRa.parsePacket()) {
    String recv = "";
    while (LoRa.available()) {
      recv += (char)LoRa.read();
    }

    TRACELN(recv);
    if (client.connected()) {
      String rs = String(LoRa.packetRssi());
      String p = "{\"code\": \"" + recv + "\",\"rssi\": " + rs + "}";
      client.publish("lora_gateway/gateway/raw", String(p).c_str(), retain);
      mqtt_lora_gateway_update();

      // Parse message
      parsePacket(recv);

      if(received_code == NewMailCode) {
        String topic = "lora_gateway/" + NewMailName;
        client.publish((topic + "/state").c_str(), "ON", retain);
        client.publish((topic + "/battery").c_str(), bat_val.c_str(), retain);
        client.publish((topic + "/rssi").c_str(), rs.c_str(), retain);
      };

      client.endPublish();
    }
  }

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (millis() > 86400000) {
    ESP.restart();
  }

}