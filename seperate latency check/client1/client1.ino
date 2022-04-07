#include "WiFi.h"
#include "esp_now.h"
#include "time.h"
#include <ESP32Time.h>

ESP32Time rtc;
const int inputpin = 5; // GPIO pin as input
const char* ssid0       = "***********";  // replace with your WIFI name
const char* password0   = "***********";  // replace with your password


// REPLACE WITH THE MAC Address of your receiver 
uint8_t broadcastAddress[] = {0x7C, 0x9E, 0xBD, 0x92, 0x39, 0x50};

// structures for receiving time stamp from master
typedef struct struct_message {
  int id;
  unsigned long epoch;
  unsigned long time_us;
}struct_message;


// Processing time on client
typedef struct struct_time_diff {
  int id;
  long time_diff; 
} struct_time_diff;


int i = 0;  // optional, not needed if initial time setting on client is not needed
int inputState = 0;

// Create a struct_message called myData
struct_message myData_rev;  // received time stamp from master
struct_message myData_sendTime; // send the timestamp in response to GPIO trigger.
struct_time_diff myData_send; // send the processing time at this client to master during latency check.

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
    i = 0;
  }
}
// Callback when data is received
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  unsigned long  time_us_aft_rev = rtc.getMicros(); // micros after request from master
  unsigned long curr_epoch = rtc.getEpoch();  // current epoch after request from master 

  // Length here used to differentiate b/w timestamp from master or latency check state of clients.
  if (len == 12){ // master sends timestamp
    // Update the structures with the new incoming data 
    memcpy(&myData_rev, incomingData, sizeof(myData_rev));
        
    if ( i == 0 ) { // This is optional but used to set time to client before latency check
      
      rtc.setTime(myData_rev.epoch, myData_rev.time_us );
      setenv("TZ", "CET-1", 1);
      tzset(); 
      
    }
    else {  // Set corrected time on clients after master estimates one way latency.
            
      rtc.setTime(myData_rev.epoch, myData_rev.time_us );
      setenv("TZ", "CET-1", 1);
      tzset();
      
    }     
  }
  else if (len == 2){  // master requests response from clients for latency check with dummy data.
    myData_send.id = 1; // client number
    i = i+1;  
    // Calculate processing time at the client node
    myData_send.time_diff = (signed long)(((rtc.getEpoch() - curr_epoch) * 1000000UL) + (rtc.getMicros()- time_us_aft_rev));
    // esp now send function   
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData_send, sizeof(myData_send));
   
    if (result == ESP_OK) {
      Serial.println("Sent with success");
    }
    else {
      Serial.println("Error sending the data");
    }
  }
}

void setup(){
  // Init Serial Monitor
  Serial.begin(115200);
  Serial.println();
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
  // Register for a callback function that will be called when data is received
  esp_now_register_recv_cb(OnDataRecv);
  
  pinMode(inputpin, INPUT); // Configure the GPIO pin as input.
}
 
void loop() {
  myData_sendTime.id = 1; // set the client number fro master to recognize the client.
  inputState = digitalRead(inputpin); // read the GPIO input pin
  if (inputState == HIGH) {
    myData_sendTime.epoch = rtc.getEpoch();
    myData_sendTime.time_us = rtc.getMicros();
    // Send message via ESP-NOW with current timestamp of the client
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData_sendTime, sizeof(myData_sendTime));
    
    if (result == ESP_OK) {
      Serial.println("Sent with success \r\n");
    }
    else {
      Serial.println("Error sending the data");
    }
    delay(1000);
  }
  else if (i < 10 && inputState == LOW){  // latency check for 10 iterations in this case.
    delay(1000); 
  }  
}
