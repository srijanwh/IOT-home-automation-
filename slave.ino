// Libraries
#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>

// Constants
#define UPDATE_TIME 500
#define BUZZER_PIN 15 // Define the buzzer pin

// Parameters
String nom = "Slave0";
const char* ssid = "ShadowPapaya";
const char* pass = "12345678";

// Define RFID Pins
#define SS_PIN 5    // Slave Select (SDA)
#define RST_PIN 17  // Reset pin
MFRC522 rfid(SS_PIN, RST_PIN); // Create MFRC522 instance

// Variables
String command;
unsigned long previousRequest = 0;

// Objects
WiFiServer server(80); // Listening server
WiFiClient master;
IPAddress serverIP(192, 168, 82, 204); // Master ESP IP
String scannedUID = "";
IPAddress ip(192, 168, 82, 202); // Slave ESP IP
IPAddress gateway(192, 168, 82, 101);
IPAddress subnet(255, 255, 255, 0);

void setup() {
  // Initialize SPI and RFID
  SPI.begin();
  rfid.PCD_Init();
  Serial.begin(115200);
  Serial.println("RFID reader initialized");

  // Init ESP32 Wifi
  WiFi.config(ip, gateway, subnet);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }
  server.begin();
  Serial.print(nom);
  Serial.print(F(" connected to Wifi! IP address : http://"));
  Serial.println(WiFi.localIP());


  // Initialize server and buzzer
  server.begin();
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // Ensure the buzzer is off initially
}

void loop() {
  listenForMasterMessage(); // Listen for any gas alert from the master

  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    scannedUID = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      scannedUID += String(rfid.uid.uidByte[i], HEX);
    }

    Serial.print("RFID UID: ");
    Serial.println(scannedUID);

    if ((millis() - previousRequest) > UPDATE_TIME) { // client connect to server every 500ms
      previousRequest = millis();

      if (master.connect(serverIP, 80)) { // Connection to the master ESP
        master.println(scannedUID + "\r");
        Serial.println("Sent UID to master: " + scannedUID);
      }
    }
    rfid.PICC_HaltA();
  }
  delay(2000);
  scannedUID = "";
}

// Function to listen for gas alert from the master
void listenForMasterMessage() {
  WiFiClient client = server.available(); // Check if there is an incoming connection
  client.setTimeout(50);
  if (client && client.connected()) {
    String message = client.readStringUntil('\r');
    Serial.println("Received message from master: " + message);

    // Trigger the buzzer if a gas alert is received
    if (message == "GAS ALERT") {
      ringBuzzer();
    }
  }
}

// Function to activate the buzzer on gas alert
void ringBuzzer() {
  Serial.println("Gas alert received! Activating buzzer.");
  digitalWrite(BUZZER_PIN, HIGH); // Turn on the buzzer
  delay(500); // Buzzer on for 2 seconds
  digitalWrite(BUZZER_PIN, LOW);
  delay(500); 
  digitalWrite(BUZZER_PIN, HIGH); // Turn on the buzzer
  delay(500); // Buzzer on for 2 seconds
  digitalWrite(BUZZER_PIN, LOW);
   delay(500); 
  digitalWrite(BUZZER_PIN, HIGH); // Turn on the buzzer
  delay(500); // Buzzer on for 2 seconds
  digitalWrite(BUZZER_PIN, LOW);  // Turn off the buzzer
}
