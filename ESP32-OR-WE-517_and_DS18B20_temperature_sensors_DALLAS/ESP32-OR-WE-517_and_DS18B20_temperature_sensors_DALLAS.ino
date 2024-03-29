#include "credentials.h"
#include <WiFi.h>               // check this later https://randomnerdtutorials.com/esp32-useful-wi-fi-functions-arduino/
#include <ESPAsyncWebServer.h> // https://randomnerdtutorials.com/esp32-async-web-server-espasyncwebserver-library/
                               // https://github.com/me-no-dev/ESPAsyncWebServer
#include <AsyncTCP.h>          // https://github.com/me-no-dev/AsyncTCP
#include <HTTPClient.h>
#include <OneWire.h> // https://randomnerdtutorials.com/esp32-ds18b20-temperature-arduino-ide/
#include <DallasTemperature.h>  // https://randomnerdtutorials.com/esp32-multiple-ds18b20-temperature-sensors/

// Data wire is connected to GPIO15
#define ONE_WIRE_BUS 15
// Setup a oneWire instance to communicate with a OneWire device
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

DeviceAddress sensor1 = { 0x28, 0xFF, 0x7A, 0xB4, 0x1, 0x17, 0x3, 0xCC };
DeviceAddress sensor2 = { 0x28, 0xFF, 0xE5, 0x2D, 0x0, 0x17, 0x3, 0xCD };
DeviceAddress sensor3 = { 0x28, 0xFF, 0xC9, 0x1D, 0x3, 0x17, 0x4, 0x3B };
DeviceAddress sensor4 = { 0x28, 0xFF, 0xF8, 0xBD, 0xD0, 0x16, 0x5, 0xBC };
DeviceAddress sensor5 = { 0x28, 0xFF, 0xC9, 0x8,  0x2, 0x17, 0x3, 0xAA };

#define RXD2 16
#define TXD2 17
float voltageL1,voltageL2,voltageL3,frequency,currentL1,currentL2,currentL3,activePowerTotal,activePowerL1,activePowerL2,activePowerL3,tempSensor1,tempSensor2,tempSensor3,tempSensor4,tempSensor5;

// Replace with your network credentials
const char* ssid = WIFI_SSID;
const char* password  = WIFI_PASSWD;

//to use IPAddress uncomment WiFi.config in Setup()
IPAddress ip(192, 168, 10, 202);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(1, 1, 1, 1);
IPAddress secondaryDNS(8, 8, 8, 8);
IPAddress gateway(192, 168, 10, 1);
String hostname = "ESP32_Power_PROD"; // https://randomnerdtutorials.com/esp32-set-custom-hostname-arduino/

// my server API where I save data to SQLite
const char* serverName = "http://192.168.10.200/MyHAM/EnergyMeter";  //https://microdigisoft.com/http-get-and-http-post-with-esp32-json-url-encoded-text-2/


AsyncWebServer server(80);

//web page with all available data reachable on http://192.168.10.200/ ...auto refresh

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP Web Server</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <style>
    html {font-family: Arial; display: inline-block; text-align: left;}
    p {font-size: 1.0rem;}
    body {max-width: 600px; vertical-align:left; padding-bottom: 25px;}
   
  </style>
  <script src="https://code.jquery.com/jquery-3.5.1.js"></script>
</head>
<body>
  <h2>ESP32 | OR-WE-517</h2>
 <div id="reloaddiv">         
   %MYDATA%
   </div>

<script>
$(document).ready(function(){
setInterval(function(){
      $("#reloaddiv").load(window.location.href + " #reloaddiv" );
}, 2000);
});

</script>
</body>
</html>
)rawliteral";


String dataToHtml(const String& var){
  //Serial.println(var);
  if(var == "MYDATA"){
    String lines = "";
      lines += "<p>L1 Voltage: " + String(voltageL1)+  "V</p>";
      lines += "<p>L2 Voltage: " + String(voltageL2)+  "V</p>";
      lines += "<p>L3 Voltage: " + String(voltageL3)+  "V</p>";
      lines += "<p>L1 Current: " + String(currentL1)+  "A</p>";
      lines += "<p>L2 Current: " + String(currentL2)+  "A</p>";
      lines += "<p>L3 Current: " + String(currentL3)+  "A</p>";
      lines += "<p>Active Power Total: " + String(activePowerTotal*1000)+  "W</p>";
      lines += "<p>L1 Active Power: " + String(activePowerL1*1000)+  "W</p>";
      lines += "<p>L2 Active Power: " + String(activePowerL2*1000)+  "W</p>";
      lines += "<p>L3 Active Power: " + String(activePowerL3*1000)+  "W</p>";
      lines += "<br>";
      lines += "<h2>ESP32 | Temperature sensors</h2>";
      lines += "<p>Sensor1:  " + String(tempSensor1)+  "°C</p>";
      lines += "<p>Sensor2:  " + String(tempSensor2)+  "°C</p>";
      lines += "<p>Sensor3:  " + String(tempSensor3)+  "°C</p>";
      lines += "<p>Sensor4:  " + String(tempSensor4)+  "°C</p>";
      lines += "<p>Sensor5:  " + String(tempSensor5)+  "°C</p>";

    return lines;
  }
  return String();
}





int i;
int sendmsglen=8; // send message for OR-WE-517 is always 8bites long 

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8E1, RXD2, TXD2);  //Important!!! ...don't use 8N1 with deveice OR-WE-517 ...check device configuration/datasheet ...https://www.arduino.cc/reference/en/language/functions/communication/serial/begin/
  delay(1000);

  
  Serial.print("\n");
  Serial.print("Connecting to: ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA); /* Configure ESP32 in STA Mode */
  //WiFi.config(ip, gateway, subnet,primaryDNS, secondaryDNS);  
  WiFi.setHostname(hostname.c_str()); 
  WiFi.begin(ssid, password); /* Connect to Wi-Fi based on the above SSID and Password */
  while(WiFi.status() != WL_CONNECTED)
  {
    Serial.print("*");
    delay(100);
  }
  Serial.print("\n");
  Serial.print("Connected to Wi-Fi: ");
  Serial.println(WiFi.SSID());
  Serial.println(WiFi.localIP());
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  delay(100);


  delay(2000);
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html,dataToHtml);
  });

// just for testing
 server.on("/hello", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hello World!");
  });

// just for monitoring
  server.on("/status", HTTP_GET, [] (AsyncWebServerRequest *request) {
       request->send(200, "text/plain", "OK");
  });

  // Start server
  server.begin();
  
  sensors.begin();
}

float From32HexToDec (byte val1, byte val2, byte val3, byte val4){
  float x;
  ((byte*)&x)[3]= val1;
  ((byte*)&x)[2]= val2;
  ((byte*)&x)[1]= val3;
  ((byte*)&x)[0]= val4;
  //Serial.println(x);
  return x;
}  


void loop() {
 Serial.print("Requesting temperatures...");
  sensors.requestTemperatures(); // Send the command to get temperatures
  Serial.println("DONE");

  tempSensor1 = sensors.getTempC(sensor1);
  Serial.print("Sensor 1(*C): ");
  Serial.println(tempSensor1); 
 
 tempSensor2 = sensors.getTempC(sensor2);
  Serial.print("Sensor 2(*C): ");
  Serial.println(tempSensor2); 
  
  tempSensor3 = sensors.getTempC(sensor3);
  Serial.print("Sensor 3(*C): ");
  Serial.println(tempSensor3); 
  
  tempSensor4 = sensors.getTempC(sensor4);
  Serial.print("Sensor 4(*C): ");
  Serial.println(tempSensor4); 
    
  tempSensor5 = sensors.getTempC(sensor5);
  Serial.print("Sensor 5(*C): ");
  Serial.println(tempSensor5); 
 Serial.println();
 
// ************************  SEND DATA TO DEVICE ************************* 
// https://unserver.xyz/modbus-guide/#modbus-rtu-data-frame-section
// CRC calculator https://www.simplymodbus.ca/crc.xls
// OR-WE-517 modbus registers list https://b2b.orno.pl/download-resource/26065/
// byte sendmsg[] = {0x01, 0x03, 0x00, 0x1C, 0x00, 0x01, 0x45, 0xCC};
//byte sendmsg[] = {0x01, 0x03, 0x00, 0x1C, 0x00, 0x02, 0x05, 0xCD}; 

byte sendmsg[] = {0x01, 0x03, 0x00, 0x0E, 0x00, 0x16, 0xA5, 0xC7}; 

Serial.print("Message sent to device: ");
for(i = 0 ; i < sendmsglen ; i++){
  Serial2.write(sendmsg[i]); 
  Serial.print(sendmsg[i], HEX); 
 }

// This part is commented as I used it during development and it is not needed later.
// Results looks like this:

// Message sent to device: 1301C025CD
// Message byte array
// Bite number:0|1|2|3|4|5|6|7|
// HEX :1|3|0|1C|0|2|5|CD|
// DEC :1|3|0|28|0|2|5|205|

// Uncoment when needed
// Serial.println();
// Serial.println("Message byte array");
// Serial.print("Bite number:");
//  for(i = 0 ; i < sendmsglen ; i++){
//       Serial.print(i);  
//             Serial.print("|");      
//  }
//  Serial.println();
//   Serial.print("HEX :");
//  for(i = 0 ; i < sendmsglen ; i++){
//       Serial.print(sendmsg[i], HEX);  
//             Serial.print("|");      
//  }
//   Serial.println();
//    Serial.print("DEC :");
//  for(i = 0 ; i < sendmsglen ; i++){
//       Serial.print(sendmsg[i]);  
//             Serial.print("|");      
//  }
// Serial.println();
Serial.println();


// **********************  DATA RECEIVED FROM DEVICE **********************
int a=0;
int b=0;
byte receivedmsg[50];

while(Serial2.available()) {
 receivedmsg[a] = Serial2.read();
 a++;
 }
Serial.print("Data received from device: ");
for(b = 0 ; b < a ; b++){      
 Serial.print(receivedmsg[b], HEX); 
 }
Serial.println();

// This part is commented as I used it during development and it is not needed later.
// Results looks like this:

// Data received: 1343EE66666BDA6
// Message byte array
// Bite number:0|1|2|3|4|5|6|7|8|
// HEX :1|3|4|3E|E6|66|66|BD|A6|
// DEC :1|3|4|62|230|102|102|189|166|

// Uncoment when needed
// Serial.println("Message byte array");
// Serial.print("Bite number:");
//  for(b = 0 ; b < a ; b++){
//   Serial.print(b);  
//   Serial.print("|");      
//  }
// Serial.println();
// Serial.print("HEX :");
//  for(b = 0 ; b < a ; b++){
//   Serial.print(receivedmsg[b], HEX);  
//   Serial.print("|");      
//  }
// Serial.println();
// Serial.print("DEC :");
// for(b = 0 ; b < a ; b++){
//   Serial.print(receivedmsg[b]);  
//   Serial.print("|");      
//  }
// Serial.println(); 
Serial.println();


// ************************************************************************
// Get DATA part from byte message

// We get message for 6 registers but each register of device OR-WE-517 contain from 4 DATA bits.   
// Example:                         1       2      3
// Data received from device: 13C 437200 437200 43734CCD 301A
// Below is whole magic. I use function From32HexToDec to do is nice way.
// Testing https://babbage.cs.qc.cuny.edu/IEEE-754.old/32bit.html
  // float x;
  // ((byte*)&x)[3]= receivedmsg[3];
  // ((byte*)&x)[2]= receivedmsg[4];
  // ((byte*)&x)[1]= receivedmsg[5];
  // ((byte*)&x)[0]= receivedmsg[6];
  // Serial.println(x,2);
  // Serial.println(From32HexToDec(receivedmsg[3],receivedmsg[4],receivedmsg[5],receivedmsg[6]),2);




voltageL1 =From32HexToDec(receivedmsg[3],receivedmsg[4],receivedmsg[5],receivedmsg[6]);
Serial.print("L1 Voltage: ");
Serial.print(voltageL1,2);
Serial.println(" V");
voltageL2 =From32HexToDec(receivedmsg[7],receivedmsg[8],receivedmsg[9],receivedmsg[10]);
Serial.print("L2 Voltage: ");
Serial.print(voltageL2,2);
Serial.println(" V");
voltageL3 =From32HexToDec(receivedmsg[11],receivedmsg[12],receivedmsg[13],receivedmsg[14]);
Serial.print("L3 Voltage: ");
Serial.print(voltageL3,2);
Serial.println(" V");
frequency =From32HexToDec(receivedmsg[15],receivedmsg[16],receivedmsg[17],receivedmsg[18]);
Serial.print("Grid Frequency: ");
Serial.print(frequency,2);
Serial.println(" Hz");
currentL1 =From32HexToDec(receivedmsg[19],receivedmsg[20],receivedmsg[21],receivedmsg[22]);
Serial.print("L1 Current: ");
Serial.print(currentL1,2);
Serial.println(" A");
currentL2 =From32HexToDec(receivedmsg[23],receivedmsg[24],receivedmsg[25],receivedmsg[26]);
Serial.print("L2 Current: ");
Serial.print(currentL2,2);
Serial.println(" A");
currentL3 =From32HexToDec(receivedmsg[27],receivedmsg[28],receivedmsg[29],receivedmsg[30]);
Serial.print("L3 Current: ");
Serial.print(currentL3,2);
Serial.println(" A");
activePowerTotal =From32HexToDec(receivedmsg[31],receivedmsg[32],receivedmsg[33],receivedmsg[34]);
Serial.print("Total Active Power: ");
// Serial.print(activePowerTotal,2);
// Serial.print(" kW | ");
Serial.print(activePowerTotal*1000);
Serial.println(" W");
activePowerL1 =From32HexToDec(receivedmsg[35],receivedmsg[36],receivedmsg[37],receivedmsg[38]);
Serial.print("L1 Active Power: ");
// Serial.print(activePowerL1,2);
// Serial.print(" kW | ");
Serial.print(activePowerL1*1000);
Serial.println(" W");
activePowerL2 =From32HexToDec(receivedmsg[39],receivedmsg[40],receivedmsg[41],receivedmsg[42]);
Serial.print("L2 Active Power: ");
// Serial.print(activePowerL2,2);
// Serial.print(" kW | ");
Serial.print(activePowerL2*1000);
Serial.println(" W");
activePowerL3 =From32HexToDec(receivedmsg[43],receivedmsg[44],receivedmsg[45],receivedmsg[46]);
Serial.print("L3 Active Power: ");
// Serial.print(activePowerL3,2);
// Serial.print(" kW | ");
Serial.print(activePowerL3*1000);
Serial.println(" W");

Serial.println();

//Send an HTTP POST request   //https://microdigisoft.com/http-get-and-http-post-with-esp32-json-url-encoded-text-2/

    if(WiFi.status()== WL_CONNECTED){
      WiFiClient client;
      HTTPClient http;
    
      // Your Domain name with URL path or IP address with path
      http.begin(client, serverName);

      
      // If you need an HTTP request with a content type: application/json, use the following:
      http.addHeader("Content-Type", "application/json");
      int httpResponseCode = http.POST("{\"vl1\":\""+String(voltageL1)+"\",\"vl2\":\""+String(voltageL2)+"\",\"vl3\":\""+String(voltageL3)+"\",\"cl1\":\""+String(currentL1)+"\",\"cl2\":\""+String(currentL2)+"\",\"cl3\":\""+String(currentL3)+"\",\"wt\":\""+String(activePowerTotal*1000)+"\",\"wl1\":\""+String(activePowerL1*1000)+"\",\"wl2\":\""+String(activePowerL2*1000)+"\",\"wl3\":\""+String(activePowerL3*1000)+"\"}");

      if(httpResponseCode>0){
        String response = http.getString();
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        Serial.println(response);
      }else{
  
        Serial.print("Error on sending POST: ");
        Serial.println(httpResponseCode);
  
}
      // Free resources
      http.end();
    }
    else {
      Serial.println("WiFi Disconnected");
    }
 

delay(5000);        

}



