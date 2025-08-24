#include <SPI.h>
#include <RF24.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h> // We need this here to build the JSON for Supabase

// --- WiFi Credentials ---
const char* ssid = "3 idiots";
const char* password = "#3idiots#";

// --- Supabase Credentials ---
const String supabaseUrl = "https://lropzpgfrbvchmsnugza.supabase.co";
const String supabaseAnonKey = "YOUR_SUPABASE_ANON_KEY"; // Copied for completeness
const String tableName = "sound_data";

// --- NEW: Fixed-size Data Structure ---
// This must be IDENTICAL to the one on the Arduino.
struct SensorData {
  float dbA;
  float dbB;
  float dbC;
  float dbD;
  bool isABRed;
};

// --- NRF24L01 Radio Configuration ---
RF24 radio(4, 5); // CE, CSN
const byte address[6] = "00001";

// Forward declaration
void sendDataToSupabase(SensorData data);

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Receiver Initializing...");

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: "); Serial.println(WiFi.localIP());

  // --- Initialize NRF24L01 Radio (using fire bot method) ---
  radio.begin();
  radio.setPALevel(RF24_PA_LOW);
  radio.setChannel(100);          // Must be the same channel as the transmitter
  radio.openReadingPipe(1, address); // Use pipe 1 for receiving
  radio.startListening();
  Serial.println("NRF24L01 Radio Initialized. Listening for data packets...");
}

void loop() {
  if (radio.available()) {
    SensorData receivedData; // Create a struct to hold the incoming data
    radio.read(&receivedData, sizeof(receivedData)); // Read the fixed-size packet
   
    Serial.println("\n-------------------------");
    Serial.println("Received data packet:");
    Serial.print("  Sound A: "); Serial.println(receivedData.dbA);
    Serial.print("  Sound B: "); Serial.println(receivedData.dbB);
    Serial.print("  Sound C: "); Serial.println(receivedData.dbC);
    Serial.print("  Sound D: "); Serial.println(receivedData.dbD);
    Serial.print("  AB Road is Red: "); Serial.println(receivedData.isABRed ? "Yes" : "No");
    Serial.println("-------------------------");
   
    // Now send this structured data to Supabase
    sendDataToSupabase(receivedData);
  }
}

void sendDataToSupabase(SensorData data) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Cannot send data.");
    return;
  }

  // Step 1: Build the JSON payload from the received struct data
  StaticJsonDocument<256> doc;
  doc["soundA_level"] = data.dbA;
  doc["soundB_level"] = data.dbB;
  doc["soundC_level"] = data.dbC;
  doc["soundD_level"] = data.dbD;
  doc["current_red_road"] = data.isABRed ? "AB" : "CD"; // Convert boolean back to text

  char jsonBuffer[256];
  serializeJson(doc, jsonBuffer);
 
  // Step 2: Send the newly created JSON to Supabase
  HTTPClient http;
  String requestUrl = supabaseUrl + "/rest/v1/" + tableName;
  http.begin(requestUrl);
 
  http.addHeader("apikey", supabaseAnonKey);
  http.addHeader("Authorization", "Bearer " + supabaseAnonKey);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Prefer", "return=minimal");

  String payload = "[" + String(jsonBuffer) + "]";
 
  Serial.print("Sending payload to Supabase: ");
  Serial.println(payload);

  int httpResponseCode = http.POST(payload);

  if (httpResponseCode == 201) {
    Serial.println("=> SUCCESS: Data sent to Supabase!");
  } else {
    Serial.print("=> ERROR on sending POST: ");
    Serial.println(httpResponseCode);
    String response = http.getString();
    Serial.println("Server response: " + response);
  }

  http.end();
}
