#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <SPI.h>
#include <RH_RF69.h>

WiFiClient client;
PubSubClient mqttClient(client);

const char* server = "68.183.191.7";    // MQTT server (of your choice)

int status = WL_IDLE_STATUS;            // the Wifi radio's status

/********* For RFM69 ****************/
#define RF69_FREQ     916.0
#define RFM69_CS      15
#define RFM69_INT     5   

RH_RF69 rf69(RFM69_CS, RFM69_INT);
/************************************/

/********** For any JSON packet creation ************/
long lastMsg = 0;
//char msg[100];
//char temp[20], temp2[20], temp3[20];
int value = 0;
/************* end for JSON packet ******************/

/************ global for voltage sensor *************/
volatile float averageVcc = 0.0;
/****************************************************/

unsigned long previousMillis = 0;    
unsigned long previousMillis_clientloop = 0;
unsigned long previousMillis_subscribe = 0;    
const long interval = 300000;
const long interval_clientloop = 2000;       
const long interval_subscribe = 100;

//Buffer to decode MQTT messages
char message_buff[100];
bool debug = true;  //Display log message if True

/********** EEPROM storage**************************************************/
char ssidc[30];        //Stores the SSID
char passwordc[30];    //Stores the password
char in_use_newc[30];  //Denotes INUSE or NEW usage
char mqttTopic[30];    //MQTT Topic
char mqttTopicUp[30];  //MQTT Topic Uplink
char mqttTopicDown[30];//MQTT Topic Downlink
char slaveID[30];      //SlaveID
/**************************************************************************/

/************ Default value ************** *********************************/
const char* ssid = "MyRouter";                       // default SSID
const char* pass = "motorolaradio";                  // default password 
const char* SmartLockTopicUp = "SmartLockUp";        // default Topic uplink
const char* SmartLockTopicDown = "SmartLockDown";    // default Topic downlink
const char* Slave_ID = "88888888";                   // default slaveID
const char* Master_ID = "SmartLock";                 // default masterID
/***************************************************************************/

void setup() {
    Serial.begin(115200);
    EEPROM.begin(512);      //Starting and setting size of the EEPROM
    delay(100);

    //setup_RFM69();          //Setting up RFM69 radio
    
    //pinMode(16,OUTPUT);     //Pin 16 for LED
    pinMode(2,OUTPUT);      //Pin 2  for LED

    Serial.println("");
    Serial.println("");
    Serial.println("Checking the save data in EEPROM");
    
    String string_Ssid       = "";
    String string_Password   = "";
    String string_InUseNew   = "";
    String string_Topic      = "";
    String string_Topic_Up   = "";
    String string_Topic_Down = "";
    String string_SlaveID    = "";
    
    string_InUseNew = read_string(30,0);        //Read IN-USE/NEW 
    string_Ssid     = read_string(30,100);      //Read SSID
    string_Password = read_string(30,200);      //Read Password
    string_Topic    = read_string(30,300);      //Read Topic
    string_SlaveID  = read_string(30,400);      //Read SlaveID
    
    Serial.println("IN-USE/NEW in EEPROM : " +string_InUseNew);
    Serial.println("SSID in EEPROM : " +string_Ssid);
    Serial.println("Password in EEPROM : " +string_Password);
    Serial.println("Topic in EEPROM : " +string_Topic);
    Serial.println("");

    //Convert String to character Array
    string_Password.toCharArray(passwordc,30);
    string_Ssid.toCharArray(ssidc,30);  
    string_InUseNew.toCharArray(in_use_newc,30);
    string_Topic.toCharArray(mqttTopic,30);
    string_SlaveID.toCharArray(slaveID,30);

    if (string_InUseNew == "INUSE")
    {
          Serial.println("IN-USE");
          Serial.print("Connecting to ");
          Serial.println(string_Ssid);
          WiFi.begin(ssidc, passwordc);

          string_Topic_Up = string_Topic + "/Up";
          string_Topic_Up.toCharArray(mqttTopicUp,30);
          SmartLockTopicUp = mqttTopicUp;
          Serial.println("Topic for uplink : " +string_Topic_Up);

          string_Topic_Down = string_Topic + "/Down";
          string_Topic_Down.toCharArray(mqttTopicDown,30);
          SmartLockTopicDown = mqttTopicDown;
          Serial.println("Topic for downlink : " +string_Topic_Down);

          Master_ID = mqttTopic;  //Own ID is also referring by the topic - entered using APP as topic - this ID is at the casing of Master
          Serial.println("Master ID : " +string_Topic);
          
          Slave_ID = slaveID;     //Slave ID to talk to
          Serial.println("Slave ID : " +string_SlaveID);
          
      
    }else{  //New or After Reset

          //Default for WiFI 
          ssid = "MyRouter";             // default network SSID
          pass = "motorolaradio";        // default network password
          
          // We start by connecting to a WiFi network
          Serial.println("NEW");
          Serial.print("Connecting to ");
          Serial.println(ssid);
          WiFi.begin(ssid, pass);
          
          //defaultTopic
          SmartLockTopicUp = "SmartLock/Up";
          Serial.print("Topic for uplink : ");
          Serial.println(SmartLockTopicUp);
          
          SmartLockTopicDown = "SmartLock/Down";
          Serial.print("Topic for downlink : ");
          Serial.println(SmartLockTopicDown);

          Master_ID = "SmartLock";
          Serial.print("Master ID : ");
          Serial.println(Master_ID);
          
          Slave_ID = "88888888";
          Serial.print("Slave ID : ");
          Serial.println(Slave_ID);       
    }
    
    Serial.print("Waiting for WiFi... ");
    
    while (WiFi.status() != WL_CONNECTED) {
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


void setup_RFM69()
{
    if (!rf69.init()) {
      Serial.println("RFM69 radio init failed");
      while (1);
    }
    
    Serial.println("RFM69 radio init OK!");
    // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM (for low power module)
    // No encryption
    if (!rf69.setFrequency(RF69_FREQ)) {
      Serial.println("setFrequency failed");
    }

    // If you are using a high power RF69 eg RFM69HW, you *must* set a Tx power with the
    // ishighpowermodule flag set like this:
    rf69.setTxPower(20, true);  // range from 14-20 for power, 2nd arg must be true for 69HCW

    // The encryption key has to be the same as the one in the listener
    uint8_t key[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                      0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    rf69.setEncryptionKey(key);

    Serial.print("RFM69 radio @");  Serial.print((int)RF69_FREQ);  Serial.println(" MHz");
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
      //dtostrf( (float) 5350, 3, 0, temp );
      //root["Voltage"] = temp;
      /**************************************/

      /************** For Internal Temperature ************/
      //dtostrf( (float) 24, 3, 2, temp3 );
      //root["Temperature"] = temp3;
      /****************************************************/

      //root.printTo(msg);

      const char* msg = "I am Alive";

      Serial.println();
      Serial.println(F("Publish message : "));
      Serial.println(msg);

      mqttClient.publish(SmartLockTopicUp, msg);
  }

  if (currentMillis - previousMillis_subscribe >= interval_subscribe ) {
    previousMillis_subscribe = currentMillis;
    mqttClient.subscribe(SmartLockTopicDown);
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
      mqttClient.publish(SmartLockTopicUp, "hello world");
      // ... and resubscribe
      mqttClient.subscribe(SmartLockTopicDown);
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
  String sendtoSlave = "";
  
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

  if ( msgString.substring(0,1) == "0" ) {  //Turn OFF servo
    Serial.print("Turn OFF and rotation degree received : ");
    Serial.println( msgString.substring(5) );
  
    //Now we want to send turn OFF command and rotation degree to Slave
    sendtoSlave = "0FFFF" + String(Slave_ID) + "FFFF" + String(msgString.substring(5));  // "0FFFF<SlaveID>FFFF<RotationDegree>" 
    char sendtoSlaveCharPacket[50];
    sendtoSlave.toCharArray(sendtoSlaveCharPacket,50);
    Serial.print("Sending "); Serial.println(sendtoSlaveCharPacket);
    
    // Send the message now!
    rf69.send((uint8_t *)sendtoSlaveCharPacket, strlen(sendtoSlaveCharPacket));
    rf69.waitPacketSent();
  }

  if ( msgString.substring(0,1) == "1" ) {  //Turn ON servo
    Serial.print("Turn ON and rotation degree received : ");
    Serial.println( msgString.substring(5) );
  
    //Now we want to send turn OFF command and rotation degree to Slave
    sendtoSlave = "1FFFF" + String(Slave_ID) + "FFFF" + String(msgString.substring(5));  // "1FFFF<SlaveID>FFFF<RotationDegree>"  
    char sendtoSlaveCharPacket[50];
    sendtoSlave.toCharArray(sendtoSlaveCharPacket,50);
    Serial.print("Sending "); Serial.println(sendtoSlaveCharPacket);
    
    // Send the message now!
    rf69.send((uint8_t *)sendtoSlaveCharPacket, strlen(sendtoSlaveCharPacket));
    rf69.waitPacketSent();
  } 

  if ( msgString.substring(0,1) == "3" ) {  //For SlaveID
    Serial.print("The SlaveID received is: ");
    Serial.println( msgString.substring(5) );
    write_to_Memory(4,String(msgString.substring(5))); 
    write_to_Memory(0,String("INUSE"));   

    //Saved in EEPROM already, now construct message to send to Slave for him get to know his Master
    sendtoSlave = "3FFFF" + String(msgString.substring(5)) + "FFFF" + String(Master_ID);  // "3FFFF<SlaveID>FFFF<MasterID>"  - to make Slave know its Master
    char sendtoSlaveCharPacket[50];
    sendtoSlave.toCharArray(sendtoSlaveCharPacket,50);
    Serial.print("Sending "); Serial.println(sendtoSlaveCharPacket);
    
    // Send the message now!
    rf69.send((uint8_t *)sendtoSlaveCharPacket, strlen(sendtoSlaveCharPacket));
    rf69.waitPacketSent();
  }
  
  if ( msgString.substring(0,1) == "4" ) {  //For SSID
    Serial.print("The SSID received is: ");
    Serial.println( msgString.substring(5) );
    write_to_Memory(1,String(msgString.substring(5))); 
    write_to_Memory(0,String("INUSE"));
  }

  if ( msgString.substring(0,1) == "5" ) {  //For Password
    Serial.print("The Password received is: ");
    Serial.println( msgString.substring(5) );
    write_to_Memory(2,String(msgString.substring(5)));
    write_to_Memory(0,String("INUSE")); 
  }

  if ( msgString.substring(0,1) == "6" ) {  //RESET to NEW
    Serial.print("RESET to NEW received : ");
    Serial.println( msgString.substring(5) );
    write_to_Memory(0,String(msgString.substring(5)));    
  }

  if ( msgString.substring(0,1) == "7" ) {  //For Topic
    Serial.print("The Topic received is: ");
    Serial.println( msgString.substring(5) );
    write_to_Memory(3,String(msgString.substring(5)));
    write_to_Memory(0,String("INUSE"));
  }
  
}


void write_to_Memory(int num, String s){

  int offset;
  
  if (num == 0) //for NEW or OLD
  {
    offset = 0;
    s+=";";  //add ; to last    
  }

  if (num == 1)  //for WiFi SSID
  {
    offset = 100;
    s+=";";  //add ; to last
  }

  if (num == 2)  //for WiFi password
  {
    offset = 200;
    s+=";";  //add ; to last 
  } 

  if (num == 3)  //for WiFi password
  {
    offset = 300;
    s+=";";  //add ; to last 
  } 

  if (num == 4)  //for WiFi password
  {
    offset = 400;
    s+=";";  //add ; to last 
  } 
  
  write_EEPROM(s,offset);
  EEPROM.commit();
}


//write to memory
void write_EEPROM(String x,int pos){
  for(int n=pos;n<x.length()+pos;n++){
     EEPROM.write(n,x[n-pos]);
  }
}


//Reads a string out of memory
String read_string(int l, int p){
  String temp;
  for (int n = p; n < l+p; ++n)
    {
     if(char(EEPROM.read(n))!=';'){
      if(isWhitespace(char(EEPROM.read(n)))){
          //do nothing
        }else temp += String(char(EEPROM.read(n)));
      
     }else n=l+p;
     
    }
  return temp;
}

