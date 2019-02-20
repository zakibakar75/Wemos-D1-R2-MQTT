#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <PubSubClient.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ArduinoJson.h>

ESP8266WiFiMulti WiFiMulti;
WiFiClient client;
PubSubClient mqttClient(client);

const char* server = "68.183.191.7";    // MQTT server (of your choice)
char ssid[] = "MyRouter";           // your network SSID (name)
char pass[] = "motorolaradio";        // your network password
int status = WL_IDLE_STATUS;          // the Wifi radio's status

/********** For any JSON packet creation ************/
long lastMsg = 0;
char msg[100];
char temp[20], temp2[20], temp3[20];
int value = 0;
/************* end for JSON packet ******************/

/************ global for voltage sensor *************/
volatile float averageVcc = 0.0;
/****************************************************/

unsigned long previousMillis = 0;    
unsigned long previousMillis_clientloop = 0;
unsigned long previousMillis_subscribe = 0;    
const long interval = 5000;
const long interval_clientloop = 2000;       
const long interval_subscribe = 100;

//Buffer to decode MQTT messages
char message_buff[100];
bool debug = true;  //Display log message if True

void setup() {
    Serial.begin(115200);
    delay(100);
    pinMode(2,OUTPUT);     //Pin 2 for LED
    pinMode(15,OUTPUT);     //Pin 15 for RELAY1
    pinMode(13,OUTPUT);     //Pin 13 for RELAY2
    pinMode(12,OUTPUT);     //Pin 12 for RELAY3
    pinMode(14,OUTPUT);     //Pin 14 for RELAY4
    
    // We start by connecting to a WiFi network
    WiFiMulti.addAP(ssid,pass);

    //Serial.println();
    //Serial.println();
    Serial.print("Waiting for WiFi... ");

    while(WiFiMulti.run() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    Serial.println(F("You're connected to the network"));

    Serial.println();
    printCurrentNet();
    printWifiData();
    delay(2000);

    mqttClient.setServer(server, 1883);
    mqttClient.setCallback(callback);
}


void loop()
{
  StaticJsonBuffer<50> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();

  unsigned long currentMillis = millis();

  if (!mqttClient.connected()) {
        reconnect();
  }
 
  if (currentMillis - previousMillis_clientloop >= interval_clientloop) {
      previousMillis_clientloop = currentMillis;
      mqttClient.loop();
  }

  
  if (currentMillis - previousMillis >= interval) {  
    
      previousMillis = currentMillis;
  
      /*
      /******* Voltage Sensor ***************/
      dtostrf( (float) 5350, 3, 0, temp );
      root["Voltage"] = temp;
      /**************************************/

      /************** For Internal Temperature ************/
      dtostrf( (float) 24, 3, 2, temp3 );
      root["Temperature"] = temp3;
      /****************************************************/

      root.printTo(msg);

      Serial.println();
      Serial.println(F("Publish message : "));
      Serial.println(msg);
      mqttClient.publish("Wemos-Tx", msg);
  }

  if (currentMillis - previousMillis_subscribe >= interval_subscribe ) {
    previousMillis_subscribe = currentMillis;
    mqttClient.subscribe("Wemos-Rx");
  }  
}

void printWifiData()
{
  // print your WiFi shield's IP address
  IPAddress ip = WiFi.localIP();
  Serial.print(F("IP Address: "));
  Serial.println(ip);

  // print your MAC address
  byte mac[6];
  WiFi.macAddress(mac);
  char buf[20];
  sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X", mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
  Serial.print(F("MAC address: "));
  Serial.println(buf);
}

void printCurrentNet()
{
  // print the SSID of the network you're attached to
  Serial.print(F("SSID: "));
  Serial.println(WiFi.SSID());

  // print the received signal strength
  long rssi = WiFi.RSSI();
  Serial.print(F("Signal strength (RSSI): "));
  Serial.println(rssi);
}



void reconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.println();
    Serial.print(F("Attempting MQTT connection..."));
    // Attempt to connect
    if (mqttClient.connect("ESP8266", "", "")) {
      Serial.println(F("connected"));
      // Once connected, publish an announcement...
      mqttClient.publish("Wemos-Tx", "hello world");
      // ... and resubscribe
      mqttClient.subscribe("Wemos-Rx");
    } else {
      Serial.print(F("failed, rc="));
      Serial.print(mqttClient.state());
      Serial.println(F(" try again in 5 seconds"));
      // Wait 5 seconds before retrying
      delay(5000);

      while ( status != WL_CONNECTED) {
         Serial.print(F("Attempting to connect to WPA SSID: "));
         Serial.println(ssid);
         // Connect to WPA/WPA2 network
         status = WiFi.begin(ssid, pass);
      }
      
    }
  }
}


void callback(char* topic, byte* payload, unsigned int length) {
  int i = 0;
  if ( debug ) {
    Serial.println("Message recv =>  topic: " + String(topic));
    Serial.print("Length: ");
    Serial.println(String(length,DEC));
  }
  // create character buffer with ending null terminator (string)
  for(i=0; i<length; i++) {
    message_buff[i] = payload[i];
  }
  message_buff[i] = '\0';
  
  String msgString = String(message_buff);
  if ( debug ) {
    Serial.println("Payload: " + msgString);
  }
  
  if ( msgString == "ON_LED" ) {
    digitalWrite(2,LOW);  
  }

  if ( msgString == "OFF_LED" ) {
    digitalWrite(2,HIGH);  
  }

  if ( msgString == "OFF_R4" ) {
    digitalWrite(14,LOW);  
  }

  if ( msgString == "ON_R4" ) {
    digitalWrite(14,HIGH);  
  }

  if ( msgString == "OFF_R3" ) {
    digitalWrite(12,LOW);  
  }

  if ( msgString == "ON_R3" ) {
    digitalWrite(12,HIGH);  
  }

  if ( msgString == "OFF_R2" ) {
    digitalWrite(13,LOW);  
  }

  if ( msgString == "ON_R2" ) {
    digitalWrite(13,HIGH);  
  }

  if ( msgString == "OFF_R1" ) {
    digitalWrite(15,LOW);  
  }

  if ( msgString == "ON_R1" ) {
    digitalWrite(15,HIGH);  
  }
  
}


//void callback(char* topic, byte* payload, unsigned int length) {
//  Serial.print(F("Message arrived ["));
//  Serial.print(topic);
//  Serial.print(F("] "));
//  for (int i = 0; i < length; i++) {
//    Serial.print((char)payload[i]);
//  }
//  Serial.println();

//  // Switch on the LED if an 1 was received as first character
//  if ((char)payload[0] == '1') {
//    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
//    // but actually the LED is on; this is because
//    // it is acive low on the ESP-01)
//  } else {
//    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
//  }

//}

