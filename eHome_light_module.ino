#include <RCSwitch.h>
#include "SSD1306.h" //https://github.com/squix78/esp8266-oled-ssd1306
#include <NTPClient.h>  https://github.com/arduino-libraries/NTPClient
#include <WiFiUdp.h>  /used for NTP 

#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "esp_system.h"
#include "FS.h" //esp file system library
#include "AsyncTCP.h" //https://github.com/me-no-dev/AsyncTCP
#include "ESPAsyncWebServer.h"  //https://github.com/me-no-dev/ESPAsyncWebServer




const char* ssid     = "Yoyo_home";
const char* password = "sccsa25g";
char* serverMqtt = "192.168.1.40";
const char* host     = "192.168.1.40";
const char* url      = "/api";

const char* deviceId = "ehomeLightLiving";
AsyncWebServer server(80);

#define SW1 34
#define SW2 35
#define SW3 32

#define LAMP1 15
#define LAMP2 16
#define LAMP3 17

#define REMOTE 25   // touch pin ????


WiFiUDP ntpUDP;

// By default 'time.nist.gov' is used with 60 seconds update interval and
// no offset
NTPClient timeClient(ntpUDP);

// You can specify the time server pool and the offset, (in seconds)
// additionaly you can specify the update interval (in milliseconds).
// NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);


//bool sw1,sw2,sw3,remote_set;

int sw1_state,sw2_state,sw3_state,sw1PreviousState=LOW,sw2PreviousState=LOW,sw3PreviousState=LOW;
char sw1[25],sw2[25],sw3[25];
String sw1_remote,sw2_remote,sw3_remote;

bool set_sw1 ,set_sw2,set_sw3 ;



// Initialize the OLED display using Wire library
SSD1306  display(0x3c, 21, 22);

//#define BUFFER_SIZE 100

WiFiClient espClient;
PubSubClient client(espClient);

long lastReconnectAttempt = 0;
long lastConnection = 0;

//watcdog control variable
hw_timer_t *timer = NULL;

void IRAM_ATTR resetModule(){
    ets_printf("reboot\n");
    esp_restart();
}


//reconnect on MQTT connection lost
boolean reconnect() {
  if (client.connect("ehomeLightUnit")) {
    // Once connected, publish an announcement...
    client.publish("log","reconnected; Hello");
    // ... and resubscribe
     client.subscribe("eHomeLightUnit/sw1");
     client.subscribe("eHomeLightUnit/sw2");
     client.subscribe("eHomeLightUnit/sw3");
     client.publish("eHomeLightUnit/log","client connected");
  }
  return client.connected();
}

void sendLog(String message){

  HTTPClient http;
  String  currentTime=String(millis(), DEC);

  timeClient.update();
  String actual_time=timeClient.getFormattedTime();
  
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  
  root["type"] = message;
  root["uptime"] = currentTime;
  root["time"] = actual_time;
  root["deviceId"] = deviceId;
  
  char JSONmessageBuffer[300];

  root.prettyPrintTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
  
  
  http.begin("http://192.168.1.40:8080/api/log"); //Specify destination for HTTP request
  http.addHeader("Content-Type", "application/json"); //Specify content-type header
  //int httpResponseCode = http.POST("POSTING from ESP32"); //Send the actual POST request
  int httpResponseCode = http.POST(JSONmessageBuffer); //Send the actual POST request
  
  if(httpResponseCode>0){
   
      String response = http.getString();  //Get the response to the request
   
      Serial.println(httpResponseCode);   //Print return code
      Serial.println(response);           //Print request answer
   
  }else{
   
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
   
  }

  
}


void callback(char* topic, byte* payload, unsigned int length) {

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  if (String( (char *)topic) == "eHomeLightUnit/sw1"){
    int i;
    for (  i = 0; i<length; i++) 
      {
        sw1[i] = payload[i];
      }
        sw1[i] = '\0';
    
    set_sw1=false;

    const char *p_payload = sw1;
    sw1_remote = String( (char *)p_payload);

  
    Serial.println(sw1);
 }

  if (String( (char *)topic) == "eHomeLightUnit/sw2"){
    int i;
    for (  i = 0; i<length; i++) 
      {
        sw2[i] = payload[i];
      }
        sw2[i] = '\0';
    
    set_sw2=false;

    const char *p_payload = sw2;
    sw2_remote = String( (char *)p_payload);

  
    Serial.println(sw2);
 }




   if (String( (char *)topic) == "eHomeLightUnit/sw3"){
    int i;
    for (  i = 0; i<length; i++) 
      {
        sw3[i] = payload[i];
      }
        sw3[i] = '\0';
    
    set_sw3=false;

    const char *p_payload = sw3;
    sw3_remote = String( (char *)p_payload);

  
    Serial.println(sw3);
 }



 
  Serial.println();
}



RCSwitch mySwitch = RCSwitch();


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);


  
  pinMode(SW1, INPUT);
  pinMode(SW2, INPUT);
  pinMode(SW3, INPUT);
  pinMode(REMOTE, OUTPUT  );
  pinMode(LAMP1, OUTPUT);
  pinMode(LAMP2, OUTPUT);
  pinMode(LAMP3, OUTPUT);

  digitalWrite(LAMP1,HIGH);
  digitalWrite(LAMP2,HIGH);
  digitalWrite(LAMP3,HIGH);


  

 // We start by connecting to a WiFi network

    Serial.println();
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

    client.setServer(serverMqtt, 1883);
    client.setCallback(callback);
  
//Async web server
  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()) );
  });
 
  server.on("/reset", HTTP_POST, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain","ok");
    //delay(2000);
    esp_restart();
  });

  server.on("/hello", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain","ESP available");

});
 
  server.begin();

  timeClient.begin();
 
  // Initialising the UI will init the display too.
  display.init();
  display.flipScreenVertically();
  // The coordinates define the center of the text
  display.clear();
  display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(30, 0, "eHome");
  display.setFont(ArialMT_Plain_10);
  display.drawString(20, 26, "Ligthining module");
  display.display();


  




  mySwitch.enableTransmit(REMOTE);
  
 

  timer = timerBegin(0, 80, true); //timer 0, div 80
  timerAttachInterrupt(timer, &resetModule, true);
  timerAlarmWrite(timer, 3000000, false); //set time in us
  timerAlarmEnable(timer); //enable interrupt


  sendLog("system rebooted");

  
    
}

void loop() {


 timerWrite(timer, 0); //reset timer (feed watchdog)

 
//Post information to a remote server for logging purposes


long currentTime = millis();

if (currentTime - lastConnection >= 60000) {
  lastConnection = currentTime;

  String msg="uptime";

  sendLog(msg);


}





   if (!client.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {


    client.loop();




display.clear();
     display.setFont(ArialMT_Plain_16);

  sw1_state=digitalRead(SW1);
  sw2_state=digitalRead(SW2);
  sw3_state=digitalRead(SW3);




//  mySwitch.send("000000000000000000001111");
//  delay(1000);  
//  mySwitch.send("000000000000000000001110");
//  delay(1000);
//
//  mySwitch.send("000000000000000000000111");
//  delay(1000);  
//  mySwitch.send("000000000000000000000110");
//  delay(1000);
//
//
//  mySwitch.send("000000000000000000001011");
//  delay(1000);  
//  mySwitch.send("000000000000000000001010");
//  delay(1000);


///////SW1


  if (sw1_state==HIGH && sw1_state!=sw1PreviousState){
       Serial.println("SW High");
       digitalWrite(LAMP1,LOW);
          Serial.println("Publishing");
          client.publish("eHomeLightUnit/sw1","ON");
    
    
    sw1PreviousState=HIGH;
   
  }

  if (sw1_state==LOW && sw1_state!=sw1PreviousState){
     Serial.println("SW Low");
   digitalWrite(LAMP1,HIGH);
          Serial.println("Publishing");
          client.publish("eHomeLightUnit/sw1","OFF");
       
    
    sw1PreviousState=LOW;
    
  }




if (set_sw1==false){   //if a new remote message arrived 
  //Serial.println("processing incomming message");
  if (sw1_remote=="OFF"){
      mySwitch.send("000000000000000000001110");  //process it
      digitalWrite(LAMP1,HIGH);
      Serial.println("sw1off");
      set_sw1=true;
       client.publish("eHomeLightUnit/log","SW1 remote control");
  
  }
    if (sw1_remote=="ON"){
      mySwitch.send("000000000000000000001111");  //process it
      digitalWrite(LAMP1,LOW);
      Serial.println("sw1on");
      set_sw1=true;
       client.publish("eHomeLightUnit/log","SW1 remote control");
 
  }
 

}


///////SW2


  if (sw2_state==HIGH && sw2_state!=sw2PreviousState){
       Serial.println("SW High");
          digitalWrite(LAMP2,LOW);
          Serial.println("Publishing");
          client.publish("eHomeLightUnit/sw2","ON");
    
    
    sw2PreviousState=HIGH;
   
  }

  if (sw2_state==LOW && sw2_state!=sw2PreviousState){
     Serial.println("SW Low");
          digitalWrite(LAMP2,HIGH);
          Serial.println("Publishing");
          client.publish("eHomeLightUnit/sw2","OFF");
       
    
    sw2PreviousState=LOW;
    
  }




if (set_sw2==false){   //if a new remote message arrived 
  //Serial.println("processing incomming message");
  if (sw2_remote=="OFF"){
      mySwitch.send("000000000000000000000110");  //process it
      digitalWrite(LAMP2,HIGH);
      Serial.println("sw2off");
      set_sw2=true;
       client.publish("eHomeLightUnit/log","SW2 remote control");
     
  }
    if (sw2_remote=="ON"){
      mySwitch.send("000000000000000000000111");  //process it
      digitalWrite(LAMP2,LOW);
      Serial.println("sw2on");
      set_sw2=true;
       client.publish("eHomeLightUnit/log","SW2 remote control");

  }
  

}


//////SW3




  if (sw3_state==HIGH && sw3_state!=sw3PreviousState){
       Serial.println("SW High");
          digitalWrite(LAMP3,LOW);
          Serial.println("Publishing");
          client.publish("eHomeLightUnit/sw3","ON");
    
   
    sw3PreviousState=HIGH;
   
  }

  if (sw3_state==LOW && sw3_state!=sw3PreviousState){
     Serial.println("SW Low");
          digitalWrite(LAMP3,HIGH);
          Serial.println("Publishing");
          client.publish("eHomeLightUnit/sw3","OFF");
       
    
    sw3PreviousState=LOW;
    
  }




if (set_sw3==false){   //if a new remote message arrived 
  //Serial.println("processing incomming message");
  if (sw3_remote=="OFF"){
      mySwitch.send("000000000000000000001010");  //process it
      digitalWrite(LAMP3,HIGH);
      Serial.println("sw3off");
      set_sw3=true;
  client.publish("eHomeLightUnit/log","SW3 remote control");
  }
    if (sw3_remote=="ON"){
      mySwitch.send("000000000000000000001011");  //process it
      digitalWrite(LAMP3,LOW);
      Serial.println("sw3on");
      set_sw3=true;
      client.publish("eHomeLightUnit/log","SW3 remote control");
}
}
}  //end client connected else



 
  
}
