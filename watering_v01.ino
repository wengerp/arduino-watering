// =====================================================================
//                           global definitions
// =====================================================================

// import used library
#include <stdio.h>
#include <string.h>
#include "M5Stack.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"


// Configure the name and password of the connected wifi and your MQTT Serve host.
const char* ssid = "wepcomDMZ";
const char* password = "omegarick1234567";
const char* mqtt_server = "192.168.1.21";

// mqtt msg buffer size
#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];

// definitions for DHT11
#define DHTPIN 5          // pin of the arduino where the sensor is connected to
#define DHTTYPE DHT11     // define the type of sensor (DHT11 or DHT22)

// definitions for moisture sensor
#define MOISTUREPIN 36

// definitions for pump
#define VALVEPIN 2
bool pump = false;

// create instances  
WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE, 6);

// ui variables
float ui_temp = 0.0;
float ui_moist = 0.0;  
const char* ui_pump ="";

// declaration of local functions
void setupWifi();
void callback(char* topic, byte* payload, unsigned int length);
void reConnect();
float readTemperature();
float readMoisture();
void printStatesTexts();

// =====================================================================
//                         esp32 runtime functions
// =====================================================================
void setup()
{
  // M5
  M5.begin();  // Serial 115200 is initialized;
  M5.Power.begin();
  M5.Lcd.setTextSize(2);

  // mqtt
  setupWifi();
  client.setServer(mqtt_server, 1883);  //Sets the server details.
  client.setCallback(callback); //Sets the message callback function.

  // DTH11
  dht.begin();

  // valve
  pinMode(VALVEPIN, OUTPUT);

  // moisture sensor
  pinMode(MOISTUREPIN, INPUT);

  // prepare UI
  ui_temp = 0;
  ui_moist = 0;

  // log message
  Serial.println("Watering v01");

}


void loop()
{
  
  if (!client.connected()) {
    reConnect();
  }
  client.loop();  //This function is called periodically to allow clients to process incoming messages and maintain connections to the server.
  delay(2000);

  printStatesTexts();

  //Serial.print("==================>>>> ");
  Serial.println(ui_moist);

  // pump state
  M5.Lcd.setCursor(175, 90);
  M5.Lcd.setTextColor(BLACK);    
  M5.Lcd.printf("%5s", ui_pump);

  ui_pump = (pump  ? "on" : "off");
  M5.Lcd.setTextColor(RED);
  M5.Lcd.setCursor(175, 90);
  M5.Lcd.printf("%5s", ui_pump);

  // temperature
  M5.Lcd.setCursor(175, 130);
  M5.Lcd.setTextColor(BLACK);
  M5.Lcd.printf("%5.1f", ui_temp);

  ui_temp = readTemperature();
  M5.Lcd.setCursor(175, 130);
  M5.Lcd.setTextColor(RED);
  M5.Lcd.printf("%5.1f", ui_temp);

  // moisture
  M5.Lcd.setCursor(175, 170);
  M5.Lcd.setTextColor(BLACK);
  M5.Lcd.printf("%5.1f", ui_moist);

  ui_moist = readMoisture();
  M5.Lcd.setCursor(175, 170);
  M5.Lcd.setTextColor(RED);
  M5.Lcd.printf("%5.1f", ui_moist);

}

// =====================================================================
//                       COMMUNICATION (Wifi, mqtt)
// =====================================================================

void setupWifi() {
  delay(10);
  M5.Lcd.printf("Connecting to %s",ssid);
  WiFi.mode(WIFI_STA);  //Set the mode to WiFi station mode.  
  WiFi.begin(ssid, password); //Start Wifi connection. 

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    M5.Lcd.print(".");
  }
  M5.Lcd.printf("\nSuccess\n");
}

void reConnect() {
  while (!client.connected()) {
  M5.Lcd.print("Attempting MQTT connection...");
    if (client.connect("esp32-arduino", "test", "test")) {
      M5.Lcd.printf("\nSuccess\n");
      // Once connected, publish an announcement to the topic.  
      client.subscribe("garden_east/valve1");
    } else {
      M5.Lcd.print("failed, rc=");
      M5.Lcd.print(client.state());
      M5.Lcd.println("try again in 5 seconds");
      delay(5000);
    }
  }
  M5.Lcd.fillScreen(BLACK);
}

void callback(char* topic, byte* payload, unsigned int length) {
  int compRes;

  compRes = strncmp(topic,"garden_east/valve1",18);
  if (compRes == 0) {
    
     compRes = strncmp((const char*)payload,"valve1on",length);
     if (compRes == 0) {
      // switch valve1 on
      digitalWrite(VALVEPIN,HIGH);
      pump = true;

      Serial.println("switching valve1 on");
      client.publish("garden_east/valve1/status", "on");
      }
     compRes = strncmp((const char*)payload,"valve1off",length);
     if (compRes == 0) {
      // switch valve1 off
      digitalWrite(VALVEPIN,LOW);
      Serial.println("switching valve1 off");
      pump = false;
            client.publish("garden_east/valve1/status", "off");

      }
    }
}

// =====================================================================
//                    SENSOR FUNCTIONS (temp, moisture)
// =====================================================================

float readTemperature()
{
  // Wait two seconds between measurements as the sensor will not measure faster
                                    
  float temperature = dht.readTemperature();
  
  // validate values
  if (isnan(temperature))  {       
    Serial.println("Error while reading data!");
    return 0.0;
  }

  // send data via serial connection to pc
  Serial.print("%\t");
  Serial.print("Temperatur: ");
  Serial.print(temperature);
  Serial.println("°C");
  
  snprintf (msg, MSG_BUFFER_SIZE, "{\"temperature\": %.02f}", temperature); //Format to the specified string and store it in MSG.
  client.publish("garden_east/sensor1", msg);  //Publishes a message to the specified topic. 

  return temperature;  
}

float readMoisture()
{
  // Wait two seconds between measurements as the sensor will not measure faster
                                    
  float moisture = analogRead(MOISTUREPIN);
 
    if (isnan(moisture))  {       
      Serial.println("Error while reading data!");
      return 0.0;
  }

  moisture = 100.0 / 4500.0 * moisture;
  
  // validate values
  if (isnan(moisture)) {       
    Serial.println("Error while reading data!");
    return 0.0;
  }

  // send data via serial connection to pc
  Serial.print("%\t");
  Serial.print("Bodenfüchtigkeit: ");
  Serial.print(moisture);
  Serial.println("%");

  snprintf (msg, MSG_BUFFER_SIZE, "{\"moisture\": %.02f}", moisture); //Format to the specified string and store it in MSG.
  client.publish("garden_east/sensor2", msg);  //Publishes a message to the specified topic. 
  
  return moisture;  
}
// =====================================================================
//                           UI FUNCTIONS 
// =====================================================================
void printStatesTexts() {
  
  M5.Lcd.setTextSize(3);
  M5.Lcd.setTextColor(GREEN);
  M5.Lcd.setCursor(65, 40);
  M5.Lcd.printf("T S    P");
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(80, 55);
  M5.Lcd.printf("he      auerkraut   roject");

  M5.Lcd.setTextSize(3);
  M5.Lcd.setTextColor(YELLOW);
  M5.Lcd.setCursor(65, 90);
  M5.Lcd.printf("Pump:");
  M5.Lcd.setCursor(65, 130);
  M5.Lcd.print("Temp:");
  M5.Lcd.setCursor(65, 170);
  M5.Lcd.print("Moist:");

  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(65, 220);
  M5.Lcd.printf("Version 1.2_2022-02.21");
  M5.Lcd.setTextSize(3);

}
