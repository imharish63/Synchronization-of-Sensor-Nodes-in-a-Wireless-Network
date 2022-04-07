// master node code for one device as central node
// latency is checked seperately of each client so that no congestion takes place at the master node receive buffer.
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


// Function to calculate the average one way latency 
long average ( long * array, int len)  // assuming array is int.
{
  long sum = 0L ;  // sum will be larger than an item, long for safety.
  for (int i = 1 ; i < len ; i++)
    sum += array [i] ;
  return   sum / (len-1) ;  // average will be fractional, so float may be appropriate.
}


// Structure to send data timestamp to clients
typedef struct struct_message {
    int id; // must be unique for each sender board
    unsigned long epoch;
    unsigned long time_us;  
} struct_message;

// Proccesing time at client received from clients
typedef struct struct_time_diff {
  int id;
  long time_diff; 
} struct_time_diff;

// Calculate the client board offsets
typedef struct struct_boardoffset {
  int id;
  long board_offset; 
} struct_boardoffset;

// Dummy data sent to clients during latency check.
typedef struct struct_dummy {
  uint8_t id;
  uint8_t dummyData;  
} struct_dummy;



// Variable to store if sending data was successful
String success;

// Timestamp from each client in response to trigger
struct_message timeboard1;
struct_message timeboard2;
struct_message timeboard3;

// Create a structure to hold the processing time from each board
struct_time_diff board1;
struct_time_diff board2;
struct_time_diff board3;

// Send the time stamps to the clients to set time of clients
struct_message myData_send;
struct_message myData_send_board1;
struct_message myData_send_board2;
struct_message myData_send_board3;
struct_message myData_revTime;

// processing time received from clients
struct_time_diff myData_rev;
// dummy data to clients during latency check
struct_dummy myData_send_dummy;
// Create an array with all the structures
struct_time_diff boardsStruct[3] = {board1, board2, board3};
struct_message boardsTime[3] = {timeboard1, timeboard2, timeboard3};
struct_message boardCurrentSendTimes[3] = {myData_send_board1,myData_send_board2,myData_send_board3};
int i = 0; // iteration

// average latency structure 
typedef struct avg_latency {
  long last_ten_latencies[10] ;
  long cur_latency = 0;
  long avg_latency = 0;
  int iteration = 0;
  long boardoffset = 0;  
} avg_latency;

// to store the last 10 or more latencies fro each client
avg_latency client1;
avg_latency client2;
avg_latency client3;
avg_latency clients[3] = {client1, client2, client3}; 

uint8_t first = 1;

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
  if (len == 8){  // receive the processing time at clients during latency check
    // Round trip latency including the processing time at other board
    long time_diff_2_way= ((rtc.getEpoch() - boardCurrentSendTimes[myData_rev.id - 1].epoch) * 1000000L) \
                          +(rtc.getMicros() - boardCurrentSendTimes[myData_rev.id - 1].time_us);
    // copy incoming data
    memcpy(&myData_rev, incomingData, sizeof(myData_rev));
    boardsStruct[myData_rev.id-1].time_diff = myData_rev.time_diff;
    if (clients[myData_rev.id - 1].iteration < 10){ // no. of iterations for which data received from a client
      // calculating one way latency excluding the processing time at other board
      clients[myData_rev.id - 1].cur_latency = (((time_diff_2_way) -  (myData_rev.time_diff))/2);
      clients[myData_rev.id - 1].last_ten_latencies[clients[myData_rev.id - 1].iteration]= clients[myData_rev.id - 1].cur_latency;
      clients[myData_rev.id - 1].iteration= (clients[myData_rev.id - 1].iteration)+1;

      // to ensure all the client latencies are checked for 10 iterations each. we can increase no. of iterations needed as well. 
      i = int ((clients[0].iteration + clients[1].iteration + clients[2].iteration)/3);  
    }
  }
  else if (len == 12) { // Receive the timestamps from clients on trigger from the master
    // copy incoming data
    memcpy(&myData_revTime, incomingData, sizeof(myData_revTime));
    // calculate the offset of the client
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
  //init and get the time from NTP server
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
  myData_send.id = 0; // set the serial no. of board
  if (i == 0 && first == 1) { // optional to set the clients time initially
    myData_send.epoch = rtc.getEpoch(); // getting the current epoch 
    myData_send.time_us = rtc.getMicros(); // getting the cutrent micros seconds
    // Broadcast the time stamp to the clients via ESP NOW
    esp_err_t result = esp_now_send(0, (uint8_t *) &myData_send, sizeof(myData_send));
    first = 0;
    if (result == ESP_OK) {
      Serial.println("Sent with success \r\n");
    }
    else {
      Serial.println("Error sending the data");
    }
    delay(1000);
  }
  // latency check seperately.
  else if ( i < 10){  // latency check for 10 iterations in this case 
    // dummmy data
    myData_send_dummy.id = 0;
    myData_send_dummy.dummyData = 255;
    // Save the current time
    boardCurrentSendTimes[0].epoch = rtc.getEpoch();
    boardCurrentSendTimes[0].time_us = rtc.getMicros();
    // send dummy data to clients via ESP NOW during latency check
    esp_err_t result = esp_now_send(broadcastAddressClient1, (uint8_t *) &myData_send_dummy, sizeof(myData_send_dummy));
    if (result == ESP_OK) {
      Serial.println("Sent with success \r\n");
    }
    else {
      Serial.println("Error sending the data");
    }
    // delay was imposed in order to maintain gap b/w responses from clients at receive buffer of master.
    delay(10);
    // save current time
    boardCurrentSendTimes[1].epoch = rtc.getEpoch();
    boardCurrentSendTimes[1].time_us = rtc.getMicros();
    // send dummy data to clients via ESP NOW during latency check
    esp_err_t result1 = esp_now_send(broadcastAddressClient2, (uint8_t *) &myData_send_dummy, sizeof(myData_send_dummy));
    if (result == ESP_OK) {
      Serial.println("Sent with success \r\n");
    }
    else {
      Serial.println("Error sending the data");
    }
    // delay was imposed in order to maintain gap b/w responses from clients at receive buffer of master.
    delay(10);
    // save current time
    boardCurrentSendTimes[2].epoch = rtc.getEpoch();
    boardCurrentSendTimes[2].time_us = rtc.getMicros();
    // send dummy data to clients via ESP NOW during latency check
    esp_err_t result2 = esp_now_send(broadcastAddressClient3, (uint8_t *) &myData_send_dummy, sizeof(myData_send_dummy));
    if (result == ESP_OK) {
      Serial.println("Sent with success \r\n");
    }
    else {
      Serial.println("Error sending the data");
    }
    delay(980);
  }
  else if (i == 10 ){ // send the corrected timestamps to the clients after estimation of latency of each client
    // calculate avaerage latency of client 1
    clients[0].avg_latency = average(clients[0].last_ten_latencies, 10);
    // current time stamp of master
    myData_send.epoch = rtc.getEpoch();
    myData_send.time_us = (long)(rtc.getMicros()) + clients[0].avg_latency ;
    // check overflow of micros
    if (myData_send.time_us >= 1000000L){
      myData_send.time_us = myData_send.time_us - 1000000L;
      myData_send.epoch = myData_send.epoch +1 ;  
    }
    // send the corrected timestamp to the client 1
    esp_err_t result1 = esp_now_send(broadcastAddressClient1, (uint8_t *) &myData_send, sizeof(myData_send));
    if (result1 == ESP_OK) {
      Serial.println("Sent with success \r\n");
    }
    else {
      Serial.println("Error sending the data");      
    }

    delay(10); // optional
    // calculate avaerage latency of client 2
    clients[1].avg_latency = average(clients[1].last_ten_latencies, 10);
    // current time stamp of master
    myData_send.epoch = rtc.getEpoch();
    myData_send.time_us = (long)(rtc.getMicros()) + clients[1].avg_latency ;
    // check overflow of micros
    if (myData_send.time_us >= 1000000L){
      myData_send.time_us = myData_send.time_us - 1000000L;
      myData_send.epoch = myData_send.epoch +1 ;  
    }
    // send the corrected timestamp to the client 2
    esp_err_t result2 = esp_now_send(broadcastAddressClient2, (uint8_t *) &myData_send, sizeof(myData_send));
    if (result2 == ESP_OK) {
      Serial.println("Sent with success \r\n");
    }
    else {
      Serial.println("Error sending the data");      
    }

    delay(10); // optional
    // calculate avaerage latency of client 3
    clients[2].avg_latency = average(clients[2].last_ten_latencies, 10);
    
    // current time stamp of master
    myData_send.epoch = rtc.getEpoch();
    myData_send.time_us = (long)(rtc.getMicros()) + clients[2].avg_latency ;
    // check overflow of micros
    if (myData_send.time_us >= 1000000L){
      myData_send.time_us = myData_send.time_us - 1000000L;
      myData_send.epoch = myData_send.epoch +1 ;  
    }
    // send the corrected timestamp to the client 3
    esp_err_t result3 = esp_now_send(broadcastAddressClient3, (uint8_t *) &myData_send, sizeof(myData_send));
    if (result3 == ESP_OK) {
      Serial.println("Sent with success \r\n");
    }
    else {
      Serial.println("Error sending the data");
    }

    i = i+1;  // needed for resynchronization
    delay (980);
  }
  else {  // after synchronization
    // save current timestamp of master
    myData_send.epoch = rtc.getEpoch();
    myData_send.time_us = rtc.getMicros();

    // Toggle the GIPO output pin, used as trigger for clients.
    digitalWrite(outputPin, HIGH);
    delay(100);
    digitalWrite(outputPin, LOW);  
    
    if (i == 130){ // this is to resync the devices after an interval
      // i can be 9 if one can go forward with the initail latency check.
      i = 0; 
      // not needed if i is 10 i.e., no need to recheck latency. 
      clients[0].iteration = 0;
      clients[1].iteration = 0;
      clients[2].iteration = 0;
    }
    i = i+1;   
    delay(900);
  }
}
