// Last Update 09/10/21 18H33 Version definitive

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"
#include <Arduino_JSON.h>

// Replace with your network credentials
const char* ssid = "Freebox-372EBF";
const char* password = "mfrfzq7db9q43xzrqmv49b";

//EMCP
//const char* ssid = "INTERNET";
//const char* password = "---#####---=even_PW_is_masked";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

#define DAY 1
#define NIGHT 0

// ********** SET PIN NUMBERS **********
// 34 -> 39 only inputs
// Inputs
const int IO_Door1 = 34;
const int IO_Door2 = 17;
const int IO_LDR = 35;   // ADC1_CH
const int IO_Button = 18;

// Outputs
const int IO_Relay = 33;
const int IO_Status = 21;
const int IO_AliveLed = 32;
const int IO_Timer1 = 19;

// ********** SET VARIABLES ***********
volatile int Timer0HasOverflow = false;

int AliveLedStatus = LOW;
int AliveLedTimeStep = 0;

int LdrReadingInterval = 20;   // Time interval between two Day Status check
int LdrValue = -1;

int DayState = NIGHT;
int DayStateThresholdHigh = 2100;
int DayStateThresholdLow = 1900;

int RelayDelay=0;

int Door1HasMoved=-1;
int Door1Delay=0;
int EventHandled = false;

String DoorMathState="Closed";
String DoorCaroState="Closed";

// Json Variable
JSONVar DoorsState;

String doorsState(){
  
  if(digitalRead(IO_Door1))
    DoorMathState = "Open";
    else
    DoorMathState = "Closed";
  
  if(digitalRead(IO_Door2))
    DoorCaroState = "Open";
    else
    DoorCaroState = "Closed";
  
  DoorsState["DoorMathState"]=String(DoorMathState);
  DoorsState["DoorCaroState"]=String(DoorCaroState);

  String jsonString = JSON.stringify(DoorsState);
  return jsonString;
}

// ********** TIMER DECLARATION *******
hw_timer_t * Timer0 = NULL;


// ********** FUNCTIONS ***************
void IRAM_ATTR onTimer0Overflow(){
  Timer0HasOverflow = true;
}

void IRAM_ATTR onTimer1(){
  //digitalWrite(IO_Relais, HIGH);
}

void IRAM_ATTR SetRelay(){

}

void TimeManager(){
  if(Timer0HasOverflow == true){  // All 50ms
    Timer0HasOverflow = false;

    // ******** AliveLedTimeStep **********
    if(AliveLedTimeStep>0)
      AliveLedTimeStep = AliveLedTimeStep - 1;

    // ******** RelayTimeStep *************
    if(RelayDelay>0)
      RelayDelay = RelayDelay - 1;

    // ******** LDR Reading Timing *******
    if(LdrReadingInterval>0)
      LdrReadingInterval = LdrReadingInterval - 1;
  }
}

void SetAliveLed(){
  if(AliveLedTimeStep == 0){
    
    if(AliveLedStatus == LOW){
      AliveLedTimeStep = 2;        // high state 2x50ms=100ms
      AliveLedStatus = HIGH;
      digitalWrite(IO_AliveLed, LOW);
    }
    else{
      if(DayState == NIGHT)      // low state at night = 1900ms
        AliveLedTimeStep = 38;
      else
        AliveLedTimeStep =18;   // low state at day = 900ms
            
      AliveLedStatus = LOW;
      digitalWrite(IO_AliveLed, HIGH);
    }
  }  
}

void SetDayState(){    
  if(LdrReadingInterval==0){
    LdrReadingInterval=40;  // => 2s  for 5mn = 300s => 6000, 15mn = 900s => 18000
    LdrValue = analogRead(IO_LDR);

    if(DayState == DAY){
      if(LdrValue<DayStateThresholdLow)
        DayState=NIGHT;
    }
    else{
        if(LdrValue>DayStateThresholdHigh)
          DayState=DAY;
    }
  }
}

void CheckDoor(){
  if(Door1HasMoved == false){             // if Door did not move
    if(digitalRead(IO_Door1) == HIGH){    // if Door Open
      Door1HasMoved = true;               // Set Variable
      Door1Delay=5;                       // Set Delay 250ms
    }
  }
  else{                                    // if door has moved
    if(EventHandled == true){              // if door opening handled
      if(digitalRead(IO_Door1)==0){
        EventHandled = false;              // EventHandled reset after door is closed
        // Door1 has been closed, Picture has to be refreshed...
        notifyClientsToRefreshPictures(doorsState());
        Serial.print("Door Closed \n");
      }    
    }
    else{                                   // After 
      if(Door1Delay == 0){                  // after debounce time 
        if(digitalRead(IO_Door1)==1){       // If Door still open
          setRelayON();
          EventHandled = true;              // Set Event Handler
          // Door1 has been opened, Picture has to be refreshed...
          notifyClientsToRefreshPictures(doorsState());            
          Serial.print("Door Opened \n");
        }
      }
    } 
  }
}
  
void setRelayON(){
  if(DayState == NIGHT){
    digitalWrite(IO_Relay, LOW);  // Relay Actvation for 1s
    RelayDelay = 20;
  }
}

void setRelayOff(){
  if(RelayDelay == 0)
    digitalWrite(IO_Relay, HIGH);
}


// ************ WebSocket Server side Functions *************

void notifyClientsToRefreshPictures(String Data){
  ws.textAll(Data);
}

void handelWebSocketMessage(void *arg, uint8_t *data, size_t len){
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if(info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT){

    data[len] = 0;
    Serial.print((char*)data);
    
    if(strcmp((char*)data, "getDoorsState")==0){
      notifyClientsToRefreshPictures(doorsState());
      Serial.print("Notify Clients to Refresh \n");
    }
    else if (strcmp((char*)data, "toggle")==0){
      setRelayON();
      Serial.print("toggle \n");
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len){
  switch(type){
    case WS_EVT_CONNECT:
    break;
    case WS_EVT_DISCONNECT:
    break;
    case WS_EVT_DATA:
      handelWebSocketMessage(arg, data, len);
    break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
    break;
  }
}

void initWebSocket(){
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}


// *********** SETUP **********
void setup() {
  Serial.begin(115200);

  pinMode(IO_Door1, INPUT);
  pinMode(IO_Door2, INPUT);
  pinMode(IO_LDR, INPUT);
  pinMode(IO_Button, INPUT);

  pinMode(IO_Relay ,OUTPUT);
  pinMode(IO_Status ,OUTPUT);
  pinMode(IO_AliveLed ,OUTPUT);
  pinMode(IO_Timer1 ,OUTPUT);

  digitalWrite(IO_Relay, HIGH);
  digitalWrite(IO_Status, LOW);
  digitalWrite(IO_AliveLed, LOW);
  digitalWrite(IO_Timer1, LOW);

  // Timer Setup
  Timer0 = timerBegin(0, 80, true);
  timerAttachInterrupt(Timer0, &onTimer0Overflow, true);
  timerAlarmWrite(Timer0, 50000, true);  // Interruption toutes les 50000us soit 50ms
  timerAlarmEnable(Timer0);

  // Interruption Setup
  attachInterrupt(digitalPinToInterrupt(IO_Door1), SetRelay, HIGH);
  attachInterrupt(digitalPinToInterrupt(IO_Door2), SetRelay, RISING);

  // Initialize Wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED){
    Serial.println("WiFi Failed!");
    return;
  }

  Serial.println();
  Serial.print("ESP IP Address: http://");
  Serial.println(WiFi.localIP());

  // Initialize SPIFFS
  if(!SPIFFS.begin())
    Serial.println("An Error has occured while mounting SPIFFS");
    else
    Serial.println("SPIFFS Mounted successfully");
      
  initWebSocket();

  // Send web page to client
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });

/*  
  // Set the Door Picture
  server.on("/DoorMathState", HTTP_GET, [](AsyncWebServerRequest *request){
    if(digitalRead(IO_Door1)){
      request->send(SPIFFS, "/DoorOpen.png", "image/png");
    }else{
      request->send(SPIFFS, "/DoorClosed.png", "image/png");
    }
  });
  
  server.on("/DoorCaroState", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/DoorClosed.png", "image/png");
  });

  server.on("/on", HTTP_GET, [](AsyncWebServerRequest *request) {
    setRelayON();
    request->send(1000, "text/plain", "ok");
  });
*/
  server.serveStatic("/", SPIFFS, "/");  
  server.begin();
}


// ********* MAIN PROGRAMM **********
void loop() {
  TimeManager();
  SetDayState();
  SetAliveLed();
  CheckDoor();
  setRelayOff();
  ws.cleanupClients();
}
