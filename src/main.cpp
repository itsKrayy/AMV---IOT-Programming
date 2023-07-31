#include <Arduino.h>
#include <iostream>
#include <string>

#include "DHTStable.h" //For the DHT11 Sensor (Digital Pin)

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

#include <WebSockets.h> //For Socket.io

#include <SocketIoClient.h> //Socket.io Client - For sending the sensor values to the server

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>

// File System Library
#include <LittleFS.h>
// Arduino JSON library
#include <ArduinoJson.h>
//Wifi Manager Library
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager




/* DECLARATIONS */

#define TRIGGER_PIN 4 // 4 is D2 in the ESP8266 Board. For this device, this is assigned to the button.

#define DHT11_PIN       5

const char *fileConfig = "/amv_config.json"; // file in the filesystem flash

int portal_Timeout = 120; //set time for the Configuration Portal AP to close when inactive

DHTStable DHT;

// WiFiMulti WiFiMulti;
SocketIoClient webSocket;

/* END OF DECLARATIONS */

/* Configuration parameters */ 
// All these variables will have the value corresponding the contents of the fileConfig file in the file system
String device_id;
String server_IP;
String server_Port;
float temp_calibration;
float humid_calibration;
float airQual_calibration;
/* end of Configuration parameters */ 

const int analogInPin = A0;  // ESP8266 Analog Pin ADC0 = A0

const int trigPin = 13;
const int echoPin = 12;
const char *ssid = "GLOBEHINAY";
const char *pass = "12345677";
const char *HOST = "192.168.254.150";

void setup() {
  //Resets the whole config for this device [FOR TESTING ONLY]
  // ESP.eraseConfig();

  Serial.begin(115200);

  pinMode(TRIGGER_PIN, INPUT_PULLUP); //Makes the pin to only go LOW when the button is pushed.

  pinMode(D4, OUTPUT); // LED indicator for the device if in Calibration Mode

  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT);  // Sets the echoPin as an Input

  /* ------------------ LittleFS line of code ------------------ */

  LittleFS.begin();

  if(!LittleFS.exists(fileConfig)){
  Serial.println("AMV Config file for this device does not exist, creating...");

  File file = LittleFS.open(fileConfig, "w");

  if(!file){
      Serial.println("Could not open the AMV  file for writing");
      LittleFS.end();
      return;
  }

  auto bytesWritten = file.write("hello\n"); //tests if file is writeable

  Serial.printf("Bytes Written: %d \n", bytesWritten);

  if (bytesWritten == 0){
      Serial.println("Could not write to the file");
  }

  file.write(" { \"device_id\" : \"\", \"server_IP\" : \"\", \"server_Port\" : \"\", \"temp_calibration\" : 0, \"humid_calibration\" : 0, \"airQual_calibration\" : 0   } ");

  file.close();

  }
   
  // Parse JSON data
  DynamicJsonDocument configJson(512);

  File setupFile = LittleFS.open(fileConfig, "r");
  if (setupFile) {
    // Read file contents into a string
    String configData = setupFile.readString();
    setupFile.close();

    DeserializationError error = deserializeJson(configJson, configData); //prints out error if error existing

    // Check for parsing errors
    if (error) {
      Serial.print("Failed to parse config file: ");
      Serial.println(error.c_str());
      return;
    }

    // Extract configuration values for establishing WiFi Connection through WiFi Manager
    device_id = configJson["device_id"].as<String>();
    server_IP = configJson["server_IP"].as<String>();
    server_Port = configJson["server_Port"].as<String>();
    temp_calibration = configJson["temp_calibration"].as<float>();
    humid_calibration = configJson["humid_calibration"].as<float>();
    airQual_calibration = configJson["airQual_calibration"].as<float>();
    
  }


  /* ------------------ end of LittleFS line of code ------------------ */

  /* ------------------ WiFi Manager line of code ------------------ */

  WiFiManager wifiManager;

  //Resets the Wifi config for this device [FOR TESTING ONLY]
  // wifiManager.resetSettings();

  wifiManager.setConfigPortalTimeout(portal_Timeout); // Sets the time for the Access Point to close when no interaction with user

  String device_AP_Name = "AMV_Device-" + device_id; //Combines the template string and Device ID for the Access Point SSID name

  WiFiManagerParameter device_id_param("deviceID", "Device ID", device_id.c_str(), 50);
  WiFiManagerParameter server_ip_param("serverIPAddress", "Server IP Address", server_IP.c_str(), 50);
  WiFiManagerParameter server_port_param("serverPortNumber", "Server Port Number", server_Port.c_str(), 50);
  WiFiManagerParameter temp_calibration_param("tempCalibrationValue", "Temperature Calibration Value", String(temp_calibration).c_str(), 7);
  WiFiManagerParameter humid_calibration_param("humidCalibrationValue", "Humidity Calibration Value", String(humid_calibration).c_str(), 7);
  WiFiManagerParameter airQual_calibration_param("airQualCalibrationValue", "Air Quality Calibration Value", String(airQual_calibration).c_str(), 7);

  wifiManager.addParameter(&device_id_param);
  wifiManager.addParameter(&server_ip_param);
  wifiManager.addParameter(&server_port_param);
  wifiManager.addParameter(&temp_calibration_param);
  wifiManager.addParameter(&humid_calibration_param);
  wifiManager.addParameter(&airQual_calibration_param);

  wifiManager.autoConnect(device_AP_Name.c_str(), "12345677");

  device_id = device_id_param.getValue();
  server_IP = server_ip_param.getValue();
  server_Port = server_port_param.getValue();
  temp_calibration = atoi(temp_calibration_param.getValue());
  humid_calibration = atoi(humid_calibration_param.getValue());
  airQual_calibration = atoi(airQual_calibration_param.getValue());

  // Remove leading/trailing whitespaces in Strings
  device_id.trim();
  server_IP.trim();
  server_Port.trim();

  // Update the JSON object with the new device_id
  configJson["device_id"] = device_id;
  configJson["server_IP"] = server_IP;
  configJson["server_Port"] = server_Port;
  configJson["temp_calibration"] = temp_calibration;
  configJson["humid_calibration"] = humid_calibration;
  configJson["airQual_calibration"] = airQual_calibration;

  // Open the file in write mode to update the content
  File wifi_Manager_File_Saving_Params = LittleFS.open(fileConfig, "w");
  if (!wifi_Manager_File_Saving_Params) {
      Serial.println("Error Opening the file for writing");
      LittleFS.end();
      delay(1000);
      return;
  }

  // Serialize the updated JSON object and write it to the file
  if(serializeJson(configJson, wifi_Manager_File_Saving_Params)){
    Serial.println("Configurations Loaded");
    wifi_Manager_File_Saving_Params.close();
  }

  //Pass the Server details to the variable for the initalization of WebSocket
  const char *HOST = server_IP.c_str();
  int port = server_Port.toInt();

  webSocket.begin(HOST, port); //Initializes the Websocket with the variables

  /* ------------------ end of WiFi Manager line of code ------------------ */
  
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP()); //Prints the IP Address of the device in the connected network

  LittleFS.end(); //Stopping all LittleFS functions
  delay(5000); //Setup headstart

}

void loop() {
  LittleFS.begin();

  // Parse JSON data
  DynamicJsonDocument configJson(512);

  File file = LittleFS.open(fileConfig, "r");

  if(!file){
      Serial.println("Error Opening the file in the loop");
  }

  // Open configuration file for reading
  File configFile = LittleFS.open(fileConfig, "r");
  if (configFile) {
    // Read file contents into a string
    String configData = configFile.readString();
    configFile.close();

    DeserializationError error = deserializeJson(configJson, configData);

    // Check for parsing errors
    if (error) {
      Serial.print("Failed to parse config file: ");
      Serial.println(error.c_str());
      return;
    }

    // Extract configuration values
    device_id = configJson["device_id"].as<String>();
    server_IP = configJson["server_IP"].as<String>();
    server_Port = configJson["server_Port"].as<String>();
    temp_calibration = configJson["temp_calibration"].as<float>();
    humid_calibration = configJson["humid_calibration"].as<float>();
    airQual_calibration = configJson["airQual_calibration"].as<float>();
  }

  /* ------------------ BUTTON PUSHED/AP AND CONFIGURATION DEPLOYMENT line of code ------------------ */

  if (digitalRead(TRIGGER_PIN) == LOW) {
    digitalWrite(D4, LOW);

    Serial.println("BUTTON PUSHED!!!!!");

    WiFiManager wifiManager;

    wifiManager.setConfigPortalTimeout(portal_Timeout); // Sets the time for the Access Point to close when no interaction with user

    String device_AP_Name = "AMV_Device-" + device_id; //Combines the template string and Device ID for the Access Point SSID name

    WiFiManagerParameter device_id_param("deviceID", "Device ID", device_id.c_str(), 50);
    WiFiManagerParameter server_ip_param("serverIPAddress", "Server IP Address", server_IP.c_str(), 50);
    WiFiManagerParameter server_port_param("serverPortNumber", "Server Port Number", server_Port.c_str(), 50);
    WiFiManagerParameter temp_calibration_param("tempCalibrationValue", "Temperature Calibration Value", String(temp_calibration).c_str(), 7);
    WiFiManagerParameter humid_calibration_param("humidCalibrationValue", "Humidity Calibration Value", String(humid_calibration).c_str(), 7);
    WiFiManagerParameter airQual_calibration_param("airQualCalibrationValue", "Air Quality Calibration Value", String(airQual_calibration).c_str(), 7);

    wifiManager.addParameter(&device_id_param);
    wifiManager.addParameter(&server_ip_param);
    wifiManager.addParameter(&server_port_param);
    wifiManager.addParameter(&temp_calibration_param);
    wifiManager.addParameter(&humid_calibration_param);
    wifiManager.addParameter(&airQual_calibration_param);

    if (!wifiManager.startConfigPortal(device_AP_Name.c_str(), "12345677")) {
      Serial.println("Failed to Connect and Hit Timeout");
      delay(3000);
      ESP.restart();
      delay(5000);
    }

    device_id = device_id_param.getValue();
    server_IP = server_ip_param.getValue();
    server_Port = server_port_param.getValue();
    temp_calibration = atoi(temp_calibration_param.getValue());
    humid_calibration = atoi(humid_calibration_param.getValue());
    airQual_calibration = atoi(airQual_calibration_param.getValue());

    // Remove leading/trailing whitespaces in Strings
    device_id.trim();
    server_IP.trim();
    server_Port.trim();

    // Update the JSON object with the new device_id
    configJson["device_id"] = device_id;
    configJson["server_IP"] = server_IP;
    configJson["server_Port"] = server_Port;
    configJson["temp_calibration"] = temp_calibration;
    configJson["humid_calibration"] = humid_calibration;
    configJson["airQual_calibration"] = airQual_calibration;

    // Open the file in write mode to update the content
    File wifi_Manager_File_Saving_Params = LittleFS.open(fileConfig, "w");
    if (!wifi_Manager_File_Saving_Params) {
        Serial.println("Error Opening the file for writing");
        LittleFS.end();
        delay(1000);
        return;
    }

    // Serialize the updated JSON object and write it to the file
    if(serializeJson(configJson, wifi_Manager_File_Saving_Params)){
      Serial.println("Configurations Saved and Loaded");
      wifi_Manager_File_Saving_Params.close();
    }

    Serial.println(device_AP_Name + " Connected to Network");

    //Pass the Server details to the variable for the initalization of WebSocket
  const char *HOST = server_IP.c_str();
  int port = server_Port.toInt();

  webSocket.begin(HOST, port); //Initializes the Websocket with the variables

  } 

  /* ------------------ end of BUTTON PUSHED/AP AND CONFIGURATION DEPLOYMENT of code ------------------ */

  /* ------------------ MAIN ACTIVITY line of code ------------------ */

  int chk = DHT.read11(DHT11_PIN);
  switch (chk)
  {
    case DHTLIB_OK:  
      // Serial.print("OK,\t"); 
      break;
    case DHTLIB_ERROR_CHECKSUM: 
      Serial.print("Checksum error,\t"); 
      break;
    case DHTLIB_ERROR_TIMEOUT: 
      Serial.print("Time out error,\t"); 
      break;
    default: 
      Serial.print("Unknown error,\t"); 
      break;
  }

  //Getting the data values from the sensors
  float humid = DHT.getHumidity();
  float temp = DHT.getTemperature();
  int airQual = analogRead(analogInPin);

  //Adjusting the data values from the sensors with the calibration values configured
  humid = humid - humid_calibration;
  temp = temp - temp_calibration;
  airQual = airQual - airQual_calibration;

  //This Dynamic JSON Document will be sent out to the server, this has the calibrated values
  DynamicJsonDocument dataPacket(1024);
  dataPacket["device_id"] = device_id;
  dataPacket["temperature"] = humid;
  dataPacket["humidity"] = temp;
  dataPacket["airQuality"] = airQual;

  //Making the Dynamic JSON to be in STRING TYPE to be transmitted via Socket.io
  char buffer[1024];
  serializeJson(dataPacket, buffer);

  //Sends out the values to the server
  webSocket.loop();
  webSocket.emit("distanceInch", buffer);

  Serial.print("Device ID: ");
  Serial.println(device_id.c_str());
  Serial.print("Server IP: ");
  Serial.println(server_IP.c_str());
  Serial.print("Server Port: ");
  Serial.println(server_Port.c_str());
  Serial.print("Calibrated TEMPERATURE: ");
  Serial.println(temp);
  Serial.print("Calibrated HUMIDITY: ");
  Serial.println(humid);
  Serial.print("Calibrated AIR QUALITY: ");
  Serial.println(airQual);
  Serial.println(airQual_calibration);

  digitalWrite(D4, LOW);
  delay(250);
  digitalWrite(D4, HIGH);
  delay(250);

  /* ------------------ end of MAIN ACTIVITY of code ------------------ */


    file.close();

    LittleFS.end();

}