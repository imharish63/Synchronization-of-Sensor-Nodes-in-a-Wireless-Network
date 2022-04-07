#include "WiFi.h"
#include "esp_now.h"
#include "time.h"
#include <ESP32Time.h>

ESP32Time rtc;
const int inputpin = 5; // GPIO pin as input

const char* ssid0       = "***********";  // replace with your WIFI name
const char* password0   = "***********";  // replace with your password

// NTP server, daylight offset and GMT offset
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

uint8_t first = 1;
// REPLACE WITH THE MAC Address of your receiver 
uint8_t broadcastAddress[] = {0x7C, 0x9E, 0xBD, 0x92, 0x39, 0x50};


typedef struct struct_message {
  int id;
  unsigned long epoch;
  unsigned long time_us;
}struct_message;


int inputState = 0;
int i = 0; // current iteration

// Create a struct_message called myData
struct_message myData_sendTime;


// Variable to store if sending data was successful
String success;



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
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  
  pinMode(inputpin, INPUT); // // Configure the GPIO pin as input.
}
 
void loop() {
  myData_sendTime.id = 2; // set the client number fro master to recognize the client.
  inputState = digitalRead(inputpin); // read the GPIO input pin
  if (inputState == HIGH) {
    myData_sendTime.epoch = rtc.getEpoch();
    myData_sendTime.time_us = rtc.getMicros();
    // Send message via ESP-NOW with current timestamp of the client
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData_sendTime, sizeof(myData_sendTime));
    i = i+1;
    
    if (i == 600){
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
    
    if (result == ESP_OK) {
      Serial.println("Sent with success \r\n");
    }
    else {
      Serial.println("Error sending the data");
    }
    delay(1000);
  }  
}
