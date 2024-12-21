#if 1

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
#define PreambleLength 78
#define TxPower 20
float BAND = 868E6; // 433E6 / 868E6 / 915E6 /

bool retain = true;
String NewMailName = "mailbox";
String NewMailCode = "0xA2B2"; // For Example "0xA2B2";

String GarageDoorName = "garagedoor";
String GarageDoorCode = "A2C2"; // For Example "0xA2B2";
String GarageDoorCodeVibrate = "A2C3";

unsigned long wait_for_ack = 10; // seconds

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

bool waiting_for_ack = false;
unsigned long seconds_ack = ULONG_MAX - 2*wait_for_ack;

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  
  TRACE("MQTT Message arrived ["); TRACE(topic); TRACE("] ");
  #if SERIAL_DEBUG
    for (int i = 0; i < length; i++) {
      TRACE((char)payload[i]);
    }
    TRACELN("");
  #endif

  if (strcmp(topic, "lora_gateway/garagedoor/state") == 0) 
  {
    
    if ( strncmp((char*)payload, "ON",length) == 0 ) 
    {
      TRACELN("Garagedoor command received, sending LoRa message");
      // Send LoRa message to sensor
      int loopcounter = 0;
      LoRa.enableInvertIQ();
      if (loopcounter < 4) 
      {
        LoRa.beginPacket();
        LoRa.print(GarageDoorCode);
        LoRa.endPacket();
        delay (1);
        loopcounter++;
      }
      LoRa.disableInvertIQ();

      // Wait for ACK from sensor in loop()
      waiting_for_ack = true;
      seconds_ack = seconds;

    }

  }

}

void mqtt_reconnect() {
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

      // uptime
      t = "homeassistant/sensor/" + DEVICE_ID + "/uptime/config";
      m = "{\"device\":{\"ids\":[\"" + DEVICE_ID + "\"],\"name\":\"" + DEVICE_NAME + "\",\"manufacturer\":\"TTGO\"}," +
          "\"name\": \"Uptime\"," + 
          "\"icon\": \"mdi:timer\","
          "\"unit_of_measurement\": \"s\","
          "\"entity_category\": \"diagnostic\"," + 
          "\"unique_id\": \"" + DEVICE_ID + "_uptime\"," + 
          "\"state_topic\": \"lora_gateway/gateway/uptime\","
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
          "\"force_update\": true,"
          "\"state_topic\": \"lora_gateway/mailbox/battery\","
          "\"retain\":true}";
      client.publish(t.c_str(), m.c_str(), retain);

      // GARAGEDOOR
      t = "homeassistant/switch/garagedoor/config";
      m = "{\"device\":{\"ids\":[\"garagedoor\"],\"name\":\"garagedoor\",\"mf\":\"jrhbcn\"},"
          "\"name\": \"GarageDoor\","
          "\"icon\": \"mdi:garage\","
          "\"device_class\": \"switch\","
          "\"unique_id\": \"garagedoor_sensor\","
          "\"state_topic\": \"lora_gateway/garagedoor/state\","
          "\"command_topic\": \"lora_gateway/garagedoor/state\","
          "\"retain\":true}";
      client.publish(t.c_str(), m.c_str(), retain);

      t = "homeassistant/sensor/garagedoor/rssi/config";
      m = "{\"device\":{\"ids\":[\"garagedoor\"],\"name\":\"garagedoor\",\"mf\":\"jrhbcn\"},"
          "\"name\": \"RSSI\","
          "\"device_class\": \"signal_strength\","
          "\"unit_of_measurement\": \"dBm\","
          "\"entity_category\": \"diagnostic\","
          "\"unique_id\": \"garagedoor_rssi\","
          "\"force_update\": true,"
          "\"state_topic\": \"lora_gateway/garagedoor/rssi\","
          "\"retain\":true}";
      client.publish(t.c_str(), m.c_str(), retain);

      t = "homeassistant/sensor/garagedoor/battery/config";
      m = "{\"device\":{\"ids\":[\"garagedoor\"],\"name\":\"garagedoor\",\"mf\":\"jrhbcn\"},"
          "\"name\": \"Battery\","
          "\"device_class\": \"battery\","
          "\"unit_of_measurement\": \"%\","
          "\"entity_category\": \"diagnostic\","
          "\"unique_id\": \"garagedoor_battery\","
          "\"force_update\": true,"
          "\"state_topic\": \"lora_gateway/garagedoor/battery\","
          "\"retain\":true}";
      client.publish(t.c_str(), m.c_str(), retain);

      t = "homeassistant/binary_sensor/garagedoor/vibrate/config";
      m = "{\"device\":{\"ids\":[\"garagedoor\"],\"name\":\"garagedoor\",\"mf\":\"jrhbcn\"},"
          "\"name\": \"Vibrate\","
          "\"device_class\": \"vibration\","
          "\"entity_category\": \"diagnostic\","
          "\"unique_id\": \"garagedoor_vibrate\","
          "\"state_topic\": \"lora_gateway/garagedoor/vibrate\","
          "\"retain\":false}";
      client.publish(t.c_str(), m.c_str(), retain);

      // Subscribe to garage door state
      client.subscribe("lora_gateway/garagedoor/state");
      client.setCallback(mqtt_callback);

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
  client.publish((topic + "/uptime").c_str(), String(seconds).c_str(), retain);
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
  mqtt_reconnect();

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
  TRACELN("Starting LoRa on " + String(BAND/1000/1000) + " MHz");
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

  // Catch if Battery Percentage is not sent
  if(rawpkg.length() <= i+1){
    bat_val = "UNDEFINED";
   } else {
    // "Converts" float to int - For cosmetic reasons. Modify, if desired
    bat_val = rawpkg.substring(i+1);
    intconv = bat_val.toFloat() + 0;
    bat_val = String(intconv);
   }
  
}

void loop() {

  unsigned long current_millis = millis();

  // Increase the number of seconds
  if (current_millis - previous_millis >= 1000) {
    seconds += (current_millis - previous_millis)/1000;
    previous_millis = current_millis;
    //TRACE("Seconds = "); TRACELN(seconds);

    // Every 1h publish update
    if ( (seconds % 3601) == 0 ) {
        mqtt_lora_gateway_update();
        client.endPublish();
        TRACELN("Publish update.");
    }

    // Every 24h reset
    if ( (seconds % 86401) == 0 ) {
      TRACELN("Restarting ...");
      ESP.restart();
    }

  }

  if (waiting_for_ack)
  {
    if (seconds > seconds_ack + wait_for_ack ) 
    {
      TRACELN("Waiting for ACK for more than 10 seconds!!!!");
      //publish error
      String topic = "lora_gateway/garagedoor";
      client.publish((topic + "/error").c_str(), "1", false);
      client.endPublish();
      waiting_for_ack = false;
      seconds_ack = ULONG_MAX - 2*wait_for_ack;
    }
  }

  if (LoRa.parsePacket()) {
    String recv = "";
    while (LoRa.available()) {
      recv += (char)LoRa.read();
    }

    TRACELN(recv);

    // Parse message
    parsePacket(recv);

    String rs = String(LoRa.packetRssi());
    String p = "{\"code\": \"" + recv + "\",\"rssi\": " + rs + "}";

    if(received_code == GarageDoorCode) 
    {
      String topic = "lora_gateway/" + GarageDoorName;
      client.publish((topic + "/state").c_str(), "OFF", retain);
      if (bat_val != "UNDEFINED") {
        client.publish((topic + "/battery").c_str(), bat_val.c_str(), retain);
      }
      client.publish((topic + "/rssi").c_str(), rs.c_str(), retain);
      waiting_for_ack = false;
    };
    client.endPublish();

    if(received_code == GarageDoorCodeVibrate) 
    {
      String topic = "lora_gateway/" + GarageDoorName;
      client.publish((topic + "/vibrate").c_str(), "ON", false);
      client.publish((topic + "/rssi").c_str(), rs.c_str(), retain);
    };
    client.endPublish();

    if(received_code == NewMailCode) {
      String topic = "lora_gateway/" + NewMailName;
      client.publish((topic + "/state").c_str(), "ON", retain);
      if (bat_val != "UNDEFINED") {
        client.publish((topic + "/battery").c_str(), bat_val.c_str(), retain);
      }
      client.publish((topic + "/rssi").c_str(), rs.c_str(), retain);
    };

    client.publish("lora_gateway/gateway/raw", String(p).c_str(), retain);
    mqtt_lora_gateway_update();
    client.endPublish();
  }

  // Check if wifi is connected
  if (WiFi.status() != WL_CONNECTED) {
    setup_wifi();
  }

  // Reconnect to MQTT if connection is lost
  if (!client.connected()) {
    mqtt_reconnect();
  }
  client.loop();

}

#endif