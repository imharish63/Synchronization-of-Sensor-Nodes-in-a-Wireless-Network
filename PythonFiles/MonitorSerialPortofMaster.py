import serial
import time
import csv
import os

ser = serial.Serial()
ser.baudrate = 115200
ser.port = '/dev/ttyUSB0'
ser.setDTR(False)
ser.setRTS(False)
ser.open()
filepath = "/home/harish/Arduino/python/csvfiles/ntp.csv"
keywordList =['Board1offset','Board2offset','Board3offset']
noofWrites = 0
a = [0,0,0]
if os.path.exists (filepath):
    os.remove(filepath)
    
while True:
    
    b = ser.readline().decode('utf-8')
    print (b) 
    b=b.split()
     
    for index,word in enumerate (keywordList):
        if word in b:
            a[index]=int(b[-1])
            noofWrites +=1
       
            
            
      
    if (noofWrites == 3):
        print(a)
        if not os.path.exists (filepath):
            print("No file")
            with open(filepath, "a") as f:
                writer = csv.DictWriter(f, fieldnames = ["Board1offset","Board2offset","Board3offset"])
                writer.writeheader()
                
                writer = csv.writer(f,delimiter=",") 
                writer.writerow([a[0],a[1],a[2]])
                noofWrites =0
        else:
            with open(filepath, "a") as f:
                writer = csv.writer(f,delimiter=",")
                 
                writer.writerow([a[0],a[1],a[2]])
                noofWrites =0

ser.close()
