#include "WiFi.h"
#include "esp_now.h"
#include <ESP32Time.h>

ESP32Time rtc;
const int outputPin = 4;  // GPIO Pin as Output
const char* ssid0       = "***********";  // replace with your WIFI name
const char* password0   = "***********";  // replace with your password

// NTP server, daylight offset and GMT offset
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;


// REPLACE WITH THE MAC Address of your receiver 
uint8_t broadcastAddressClient1[] = {0x7C, 0x9E, 0xBD, 0x91, 0xEB, 0x90};
uint8_t broadcastAddressClient2[] = {0x7C, 0x9E, 0xBD, 0x91, 0xEB, 0x94};
uint8_t broadcastAddressClient3[] = {0x7C, 0x9E, 0xBD, 0x91, 0xEA, 0xA0};

// Structure example to send data
// Must match the receiver structure
typedef struct struct_message {
    int id; // must be unique for each sender board
    unsigned long epoch;
    unsigned long time_us;    
} struct_message;

// Variable to store if sending data was successful
String success;

// Create a structure to hold the readings from each board
struct_message timeboard1;
struct_message timeboard2;
struct_message timeboard3;


// Create a struct_message called myData
struct_message myData_send;
struct_message myData_revTime;


// Create an array with all the structures
struct_message boardsTime[3] = {timeboard1, timeboard2, timeboard3};

int i = 0; 

// structure to calculate the time offset of clients
typedef struct avg_latency {
  long boardoffset = 0;  
} avg_latency;

// offset of the boards
avg_latency client1;
avg_latency client2;
avg_latency client3;
avg_latency clients[3] = {client1, client2, client3}; 



// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  if (status ==0){
    success = "Delivery Success :)";
  }
  else{
    success = "Delivery Fail :(";
  }
}

// Callback when data is received
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {    
  if (len == 12) {  // Time stamps recieved from clients on trigger from master
    // Copy inmcoming data of timestamp
    memcpy(&myData_revTime, incomingData, sizeof(myData_revTime));
    // calculate the offset on the client
    clients[myData_revTime.id - 1].boardoffset= (long)(((myData_send.epoch-myData_revTime.epoch)*1000000) + (myData_send.time_us-myData_revTime.time_us));
    // Below print is needed to monitor the board offset through serial monitor using python and evaluate the offsets.
    Serial.printf("Board%doffset : %ld \r\n" ,myData_revTime.id, clients[myData_revTime.id - 1].boardoffset);
    Serial.println();
    // copy the received data to structures of individual clients (optional)
    boardsTime[myData_revTime.id - 1].id = myData_revTime.id;
    boardsTime[myData_revTime.id - 1].epoch = myData_revTime.epoch;
    boardsTime[myData_revTime.id - 1].time_us = myData_revTime.time_us;    
  } 
}

void setup(){
  // Init Serial Monitor
  Serial.begin(115200);
  Serial.println();
  // Connect to NTP server and obtain time  
  Serial.printf("Connecting to %s ", ssid0);
  WiFi.begin(ssid0, password0);
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  Serial.println(" CONNECTED");
  //init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  //disconnect WiFi as it's no longer needed
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
   
  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
  
  // Register peer
  esp_now_peer_info_t peerInfo;
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;

  // Add first peer
  memcpy(peerInfo.peer_addr, broadcastAddressClient1, 6);        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  
  // Add second peer
  memcpy(peerInfo.peer_addr, broadcastAddressClient2, 6);        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  
  // Add third peer
  memcpy(peerInfo.peer_addr, broadcastAddressClient3, 6);        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  // Register for a callback function that will be called when data is received
  esp_now_register_recv_cb(OnDataRecv);
  
  pinMode(outputPin, OUTPUT); // Configure the GPIO output pin.
}
 
void loop() {
    // save the current timestamp of master
    myData_send.epoch = rtc.getEpoch();
    myData_send.time_us = rtc.getMicros();

    // Toggle the GIPO output pin, used as trigger for clients.
    digitalWrite(outputPin, HIGH);
    delay(100);
    digitalWrite(outputPin, LOW);
      
    i = i+1;
    
    if (i == 100){  // this is to resync the devices after an interval
        i = 0;
        // Connect to NTP server and obtain time  
        Serial.printf("Connecting to %s ", ssid0);
        WiFi.begin(ssid0, password0);
        // Set device as a Wi-Fi Station
        WiFi.mode(WIFI_STA);
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
        }
        Serial.println(" CONNECTED");
        //init and get the time
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        
        //disconnect WiFi as it's no longer needed
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        
        // Set device as a Wi-Fi Station
        WiFi.mode(WIFI_STA);
  }      
    delay(900);
}
