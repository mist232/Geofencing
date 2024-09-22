#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TinyGPS++.h>
#include "esp_sleep.h"


const char* ssid = "T";
const char* password = "mmea2327";
const char* host = "script.google.com";
const int httpsPort = 443;
WiFiClientSecure client;
String GAS_ID = "AKfycbzeyeu5PDhJO0y1jxURTEv1y8OuIYUojWA9_CZ1vf99zn2rmHPPXyfd_EGM75HvxpIl";

//GPS
#define GPS_BAUDRATE 9600
#define CONSTANT 5.55
TinyGPSPlus gps;


void setup() {
  Serial.begin(115200);

  Serial2.begin(GPS_BAUDRATE);//gps
  delay(500);

  
  // Connect to WiFi
  Serial.println("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  client.setInsecure();
}

void loop() {

  readGPSData(); //gps read dynamically data
  
  float batteryVoltage = readVoltage();
  float latitude = gps.location.lat();
  float longitude = gps.location.lng();

  sendDataToGoogleSheets(latitude, longitude,4);//comment this line before final implementation as it sends data every 2 seconds before checkpoint
  /*
  Manually set coordinates for testing purposes
  float latitude = 27.47133;
  float longitude = 80.335129;*/

  // Send data to the API and Google Sheets
  int g=sendDataToApi(latitude, longitude); 

  if (g!=-1){
    sendDataToGoogleSheets(latitude, longitude,g);
    if (g==1)
    {
    esp_sleep_enable_timer_wakeup(15 * 60 * 1000000ULL); // 15 minutes in microseconds
    esp_deep_sleep_start();    }
  }

  // Wait for some time before sending next coordinates
  delay(120000); // 2 minute delay
}

//gpsreaddata
void readGPSData() {
  while (Serial2.available() > 0) {
    if (gps.encode(Serial2.read())) {
      if (gps.location.isValid()) {
        Serial.print(F("- latitude: "));
        Serial.println(gps.location.lat());

        Serial.print(F("- longitude: "));
        Serial.println(gps.location.lng());

        Serial.print(F("- altitude: "));
        if (gps.altitude.isValid())
          Serial.println(gps.altitude.meters());
        else
          Serial.println(F("INVALID"));
      } else {
        Serial.println(F("- location: INVALID"));
      }

      Serial.print(F("- speed: "));
      if (gps.speed.isValid()) {
        Serial.print(gps.speed.kmph());
        Serial.println(F(" km/h"));
      } else {
        Serial.println(F("INVALID"));
      }

      Serial.print(F("- GPS date&time: "));
      if (gps.date.isValid() && gps.time.isValid()) {
        Serial.print(gps.date.year());
        Serial.print(F("-"));
        Serial.print(gps.date.month());
        Serial.print(F("-"));
        Serial.print(gps.date.day());
        Serial.print(F(" "));
        Serial.print(gps.time.hour());
        Serial.print(F(":"));
        Serial.print(gps.time.minute());
        Serial.print(F(":"));
        Serial.println(gps.time.second());
      } else {
        Serial.println(F("INVALID"));
      }

      Serial.println();
    }
  }

  if (millis() > 5000 && gps.charsProcessed() < 10)
    Serial.println(F("No GPS data received: check wiring"));
  delay(2000);
}

// sends to backend service through api endpoint
int sendDataToApi(float lat, float lon) {
  HTTPClient http;
  String apiUrl = "https://ila567.pythonanywhere.com/classify_and_expand"; // Endpoint URL for backend services
  String payload = "lat=" + String(lat, 6) + "&lon=" + String(lon, 6);
  http.begin(apiUrl);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded"); // Set the content type to form-urlencoded

  int httpResponseCode = http.POST(payload);
  if (httpResponseCode > 0) {
    String response = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, response);

    const char* status = doc["status"];
    if (strcmp(status, "safe") == 0) {
      Serial.println("Location is safe.");
      return 1;
    } else if (strcmp(status, "unsafe") == 0) {
      Serial.println("Location is unsafe.");
      return 0;
    } else {
      Serial.println("Unknown status received.");
      //k = -1; // Set k to an invalid value to indicate an unknown status
    }
  } else {
    Serial.print("Error on sending POST: ");
    Serial.println(httpResponseCode);
    return -1;
  }
  http.end();
}

// sends data to google sheets
void sendDataToGoogleSheets(float lat, float lon,int k) {
    float batteryVoltage = readVoltage();
    String batteryV = String(batteryVoltage, 2); //battery check

  Serial.println("Sending data to Google Sheets");

  if (!client.connect(host, httpsPort)) {
    Serial.println("Connection to Google Sheets failed");
    return;
  }  
  String url = "/macros/s/" + GAS_ID + "/exec?latitude=" + String(lat, 6) + "&longitude=" + String(lon, 6)+ "&status="+String(k)+"&voltage="+ batteryV;
  Serial.print("Requesting URL: ");
  Serial.println(url);
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n");

  while (client.connected()) {
    if (client.available()) {
      String line = client.readStringUntil('\r');
      Serial.print(line);
    }
    
  }

  client.stop();
  Serial.println("Data sent successfully");
}

//battery voltage check function
float readVoltage() {
  float ADCvoltage = (float)analogRead(36) / 4095.0 * CONSTANT;
  return ADCvoltage;
}