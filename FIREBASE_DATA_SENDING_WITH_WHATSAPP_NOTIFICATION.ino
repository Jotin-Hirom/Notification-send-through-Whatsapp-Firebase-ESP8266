#include <Arduino.h>         // Includes core Arduino functions
#include <ESP8266WiFi.h>     // Library for WiFi connection on ESP8266
#include <FirebaseESP8266.h> // Firebase ESP8266 library for Firebase integration
#include <Adafruit_Sensor.h> // Base class for sensor library
#include <DHT.h>             // Library for DHT temperature and humidity sensor
#include <ESP8266HTTPClient.h> // HTTP client library for ESP8266
#include <WiFiClient.h>      // Allows handling WiFi clients
#include <UrlEncode.h>       // Allows URL encoding of strings

// Define pin numbers for various components
#define trigger_pin D0       // Trigger pin for ultrasonic sensor
#define echo_pin D1          // Echo pin for ultrasonic sensor
#define BUZZER D2            // Pin for buzzer
#define DHTPIN D3            // Pin for DHT sensor
#define DHTTYPE DHT11        // Define DHT type as DHT11
#define LDR A0               // Pin for light-dependent resistor (LDR)

// Define Firebase host and auth keys (hidden for security)
#define FIREBASE_HOST  //HIDDEN
#define FIREBASE_AUTH //HIDDEN

// Define WiFi credentials
#define WIFI_SSID "Lab 11"           // WiFi SSID
#define WIFI_PASSWORD "21212121"     // WiFi password

// Initialize Firebase data object
FirebaseData firebaseData;

// Initialize DHT sensor
DHT dht(DHTPIN, DHTTYPE);

// Global variables for phone number, API key, and other values (hidden for security)
String phoneNumber = //HIDDEN;
String apiKey = //HIDDEN;

// Variables for sensor readings
int value = 0;                  // Placeholder variable
long duration;                  // Holds pulse duration from ultrasonic sensor
int distance;                   // Calculated distance from ultrasonic sensor
int j = 0, i = 0;               // Counters
bool checkDHT = true;           // Flag for DHT sensor alerts
bool checkUltra = true;         // Flag for ultrasonic sensor alerts

void setup() {
  Serial.begin(115200); // Begin serial communication with baud rate 115200
  delay(500);           // Delay for stabilization

  // Scan available Wi-Fi networks
  int Tnetwork = 0, i = 0, len = 0;
  Tnetwork = WiFi.scanNetworks(); // Store number of networks found
  Serial.print(Tnetwork);         // Print total networks found
  for (int i = 0; i < Tnetwork; ++i) { // Loop to print each network's SSID and signal strength
    Serial.print(" ");
    Serial.print(WiFi.SSID(i));
    Serial.print(" ");
    Serial.print(WiFi.RSSI(i));
    Serial.println();
  }
  Serial.println(" ");
  delay(500);

  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("WIFI_SSID : ");
  Serial.print(WIFI_SSID);
  Serial.println();

  Serial.println("Connecting to Wi-Fi...");
  // Loop until Wi-Fi is connected
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);            // Wait 1 second
    Serial.print(++j);      // Print connection attempt count
    Serial.print(' ');
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP()); // Print local IP address once connected
  Serial.println();

  // Set pin modes for components
  pinMode(BUZZER, OUTPUT);
  pinMode(trigger_pin, OUTPUT);
  pinMode(echo_pin, INPUT);
  pinMode(LDR, INPUT);
  dht.begin();               // Initialize DHT sensor
  Serial.print("DHT11 tested");
  Serial.println();

  // Initialize Firebase connection
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);
  Serial.printf("Firebase Connected. \n");
  Serial.println();
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  updateSensors();       // Call updateSensors function to read and process sensor data
  delay(250);            // Delay to control sensor update rate
}

void updateSensors() {
  Serial.println("SENSORS VALUE UPDATING...");

  // Ultrasonic sensor reading
  delay(250);
  digitalWrite(trigger_pin, HIGH);  // Set trigger pin to HIGH
  digitalWrite(trigger_pin, LOW);   // Immediately set trigger pin to LOW
  duration = pulseIn(echo_pin, HIGH); // Read the pulse duration from echo pin
  distance = duration * 0.034 / 2;    // Calculate distance in cm

  // Update distance data to Firebase
  Firebase.setFloat(firebaseData, F("Mac Japan/Sensors/Ultrasonic/distance"), distance) ? "ok distance" : firebaseData.errorReason().c_str();
  
  // Check distance for alert
  if (distance > 10 && distance < 50) {
    if (checkUltra) {
      sendMessage("Someone is entering with a distance of less than 50cm."); // Send alert message
      checkUltra = false;
    }
  } else if (distance > 1 && distance < 10) {
    sendMessage("Alert!!! Someone is entering with a distance of less than 10cm. Buzzer is turn on.");
    delay(250);
    digitalWrite(BUZZER, LOW);  // Activate buzzer
  } else {
    checkUltra = true;
    digitalWrite(BUZZER, HIGH); // Deactivate buzzer
  }
  delay(250);

  // Read LDR value
  float ldr = analogRead(LDR);
  Firebase.setFloat(firebaseData, F("Mac Japan/Sensors/LDR/value"), ldr) ? "ok value of LDR" : firebaseData.errorReason().c_str();

  // DHT11 temperature and humidity sensor
  float h = dht.readHumidity();          // Read humidity
  float t = dht.readTemperature();       // Read temperature in Celsius

  // Check if any reads failed
  if (isnan(h) || isnan(t)) {
    Serial.println(F("Failed to read from DHT sensor!"));
  } else {
    float hic = dht.computeHeatIndex(t, h, false); // Calculate heat index
    Serial.print(F("Humidity: "));
    Serial.print(h);
    Serial.print(F("%  Temperature: "));
    Serial.print(t);
    Serial.print(F("°C Heat index: "));
    Serial.print(hic);
    Serial.print(F("°C "));

    // Update DHT11 data to Firebase
    Firebase.setFloat(firebaseData, F("Mac Japan/Sensors/DHT11/Temperature"), t) ? "ok temperature" : firebaseData.errorReason().c_str();
    delay(250);
    Firebase.setFloat(firebaseData, F("Mac Japan/Sensors/DHT11/Heat Index"), hic) ? "ok Heat index" : firebaseData.errorReason().c_str();
    delay(250);
    Firebase.setFloat(firebaseData, F("Mac Japan/Sensors/DHT11/Humidity"), h) ? "ok humidity" : firebaseData.errorReason().c_str();
    delay(250);

    // Temperature threshold alert
    if (t > 30) {
      if (checkDHT) {
        sendMessage("Temperature is higher than 30. Do you want to turn on the device?");
        checkDHT = false;
      }
    } else {
      checkDHT = true;
    }
  }
}

// Function to send alert message via HTTP POST
void sendMessage(String message) {
  // Prepare URL for message
  String url = "http://api.callmebot.com/whatsapp.php?phone=" + phoneNumber + "&apikey=" + apiKey + "&text=" + urlEncode(message);
  WiFiClient client;
  HTTPClient http;
  http.begin(client, url);  // Initialize HTTP connection

  http.addHeader("Content-Type", "application/x-www-form-urlencoded"); // Add headers
  int httpResponseCode = http.POST(url);  // Send POST request

  // Check response status
  if (httpResponseCode == 200) {
    Serial.print("Message sent successfully");
  } else {
    Serial.println("Error sending the message");
    Serial.print("HTTP response code: ");
    Serial.println(httpResponseCode);
  }

  http.end();  // Close HTTP connection
}
