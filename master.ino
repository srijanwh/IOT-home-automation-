/*#define BLYNK_TEMPLATE_ID "TMPL3tuW9HKxy"
#define BLYNK_TEMPLATE_NAME "capstone"
#define BLYNK_AUTH_TOKEN "2m2vDLZmtr6KzyPSGNuaSdgGSp4SqfO-"*/

//#define BLYNK_PRINT Serial
#include <WiFi.h>
//#include <BlynkSimpleEsp32.h>
#include <SPI.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <HTTPClient.h>
//Constants
#define NUM_SLAVES 1

String scannedUID = "";
int i = 0;

//Parameters
String nom = "Master";
const char* ssid = "ShadowPapaya";
const char* pass = "12345678";

//Variables
bool sendCmd = false;
String slaveCmd = "0";
String slaveState = "0";

//Objects
WiFiServer server(80);
IPAddress ip(192, 168, 82, 204);    // Master ESP IP
IPAddress slaveIP(192, 168, 82, 202); // Slave ESP IP
IPAddress gateway(192, 168, 82, 101);
IPAddress subnet(255, 255, 255, 0);

const char* serverName = "https://script.google.com/macros/s/AKfycbwoLzFlRuC2n-gBXKyZva5jRTS6uz_B1Smgevc00AmeNcVQNHlqwehmBMc6Y26tcy3oHA/exec";

// Define DHT11 Pin and Type
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// LCD Setup
LiquidCrystal_I2C lcd(0x27, 16, 2); // Initialize with address 0x27 and 16x2 size

// LDR and MQ3 Pin
#define LDR_PIN 36 // LDR connected to analog pin 36 (A0 on ESP32)
#define MQ3_PIN 34 // MQ3 connected to analog pin 34

// Define Relay Pins
#define RELAY1_PIN 13 // Relay 1 connected to GPIO 13
#define RELAY2_PIN 12 // Relay 2 connected to GPIO 12

// Define LED Pins
#define LED1_PIN 14 // LED1 connected to GPIO 14
#define LED2_PIN 27 // LED2 connected to GPIO 27
#define LED3_PIN 26 // LED3 connected to GPIO 26
#define LED4_PIN 25 // LED4 connected to GPIO 25

// Threshold for MQ3 gas sensor
#define MQ3_THRESHOLD 500 // Define your threshold here

// Variable to track whether RFID is verified
bool isRFIDVerified = false;

// Define a valid RFID UID (replace with the actual UID you want to use)
String validUID = "4e0e2a7";  // Example UID, replace with your valid RFID UID

void setup() {
  Serial.begin(115200);  // Initialize the Serial Monitor

  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();

  // Init ESP32 Wifi
  //WiFi.config(ip, gateway, subnet);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }
  server.begin();
  Serial.print(nom);
  Serial.print(F(" connected to Wifi! IP address : http://"));
  Serial.println(WiFi.localIP());

  // Initialize DHT11 sensor
  dht.begin();

  // Set relay pins as outputs
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);

  // Set LED pins as outputs
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(LED3_PIN, OUTPUT);
  pinMode(LED4_PIN, OUTPUT);

  // Turn off relays and LEDs initially
  digitalWrite(RELAY1_PIN, HIGH);
  digitalWrite(RELAY2_PIN, HIGH);
  digitalWrite(LED1_PIN, LOW);
  digitalWrite(LED2_PIN, LOW);
  digitalWrite(LED3_PIN, LOW);
  digitalWrite(LED4_PIN, LOW);
}

void loop() {
  clientRequest();

  if (i % 2 == 1) {
    // Read DHT11 sensor data
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();

    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("Failed to read from DHT sensor!");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("DHT Read Error");
      return;
    }

    // Print DHT11 data to Serial Monitor
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.print(" %\t");
    Serial.print("Temperature: ");
    Serial.println(temperature);

    // Relay control based on temperature
    if (temperature >= 31.0) {
      digitalWrite(RELAY1_PIN, LOW); // Turn on relay 2 when temperature >= 32°C
    } else {
      digitalWrite(RELAY1_PIN, HIGH);  // Turn off relay 2 otherwise
    }

    // Read LDR value (0 to 4095 for ESP32)
    int ldrValue = analogRead(LDR_PIN);
    int ldrPercentage = map(ldrValue, 0, 4095, 0, 100);

    // Print LDR value to Serial Monitor
    Serial.print("LDR Value (0-100%): ");
    Serial.println(ldrPercentage);

    // LED control based on LDR value
    digitalWrite(LED1_PIN, ldrPercentage < 60 ? HIGH : LOW);
    digitalWrite(LED2_PIN, ldrPercentage < 50 ? HIGH : LOW);
    digitalWrite(LED3_PIN, ldrPercentage < 40 ? HIGH : LOW);
    digitalWrite(LED4_PIN, ldrPercentage < 30 ? HIGH : LOW);

    // Read MQ3 sensor value
    int mq3Value = analogRead(MQ3_PIN);
    Serial.print("MQ3 Sensor Value: ");
    Serial.println(mq3Value);

    // Check if MQ3 value exceeds the threshold and send an alert
    if (mq3Value > MQ3_THRESHOLD) {
      Serial.println("Gas level exceeded threshold!");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Gas Alert!!!!!");
      sendGasAlertToSlave();
      delay(1000);
    }

    if (!isnan(humidity) && !isnan(temperature)) {
    sendDataToGoogle("ESP2", scannedUID, String(temperature), String(humidity), String(ldrValue), String(mq3Value));
  } else {
    Serial.println("Failed to read from DHT sensor!");
  }

    // Display temperature and humidity on LCD for 1 second
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Humidity: ");
    lcd.print(humidity);
    lcd.setCursor(0, 1);
    lcd.print("Temp: ");
    lcd.print(temperature);
    lcd.print(" C");
    delay(1000);

  } else {
    // If RFID is not verified, turn off LEDs and relays
    digitalWrite(RELAY1_PIN, HIGH);
    digitalWrite(RELAY2_PIN, HIGH);
    digitalWrite(LED1_PIN, LOW);
    digitalWrite(LED2_PIN, LOW);
    digitalWrite(LED3_PIN, LOW);
    digitalWrite(LED4_PIN, LOW);
  }
}

// Function to send gas alert to the slave ESP
void sendGasAlertToSlave() {
  WiFiClient client;
  if (client.connect(slaveIP, 80)) {
    client.println("GAS ALERT\r");
    Serial.println("Sent gas alert to slave");
  }
  client.stop();
}

void clientRequest() {
  WiFiClient client = server.available();
  client.setTimeout(50);
  if (client) {
    if (client.connected()) {
      Serial.print(" ->");
      Serial.println(client.remoteIP());
      String request = client.readStringUntil('\r');
      Serial.println(request);
      scannedUID = request;
      Serial.print("RFID UID: ");
      Serial.println(scannedUID);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Tag ID: ");
      lcd.print(scannedUID);
      delay(1000);
      if (scannedUID == validUID) {
        isRFIDVerified = true;
        Serial.println("RFID Verified!");
      } else {
        isRFIDVerified = false;
        Serial.println("RFID Not Verified!");
      }
      if (isRFIDVerified) {
        i = i + 1;
      }
      if (i % 2 == 1) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Welcome!");
        delay(1000);
        digitalWrite(RELAY2_PIN, LOW);
        delay(1500);
        digitalWrite(RELAY2_PIN, HIGH);
      } else if ((i % 2 == 0) && i != 0) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Bye-Bye!");
        delay(1000);
        lcd.clear();
      }
    }
  }
}

void sendDataToGoogle(String device, String uid, String temp, String humidity, String ldr, String mq3) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");

    String jsonPayload = "{\"device\":\"" + device + "\", \"scannedUID\":\"" + uid + "\", \"temperature\":\"" + temp + "\", \"humidity\":\"" + humidity + "\", \"ldrValue\":\"" + ldr + "\", \"mq3Value\":\"" + mq3 + "\"}";
    
    int httpResponseCode = http.POST(jsonPayload);
    
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println(httpResponseCode);
      Serial.println(response);
    } else {
      Serial.println("Error sending data to Google Sheets");
    }

    http.end();
  }
}
