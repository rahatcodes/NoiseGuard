#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Wire.h>
#include <SoftWire.h>
#include <RtcDS3231.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- Pin Definitions (CORRECT AND COMPLETE) ---
const int SENSOR_A_PIN = A0;
const int SENSOR_B_PIN = A1;
const int SENSOR_C_PIN = A2;
const int SENSOR_D_PIN = A3;
const int AB_RED_LED_PIN = 2;
const int AB_GREEN_LED_PIN = 3;
const int CD_RED_LED_PIN = 4;
const int CD_GREEN_LED_PIN = 5;
const int NRF_CE_PIN = 9;
const int NRF_CSN_PIN = 10;
#define RTC_SDA_PIN 7
#define RTC_SCL_PIN 6

// --- NEW: Fixed-size Data Structure ---
// This struct will be sent instead of JSON.
// It must be IDENTICAL on the ESP32.
struct SensorData {
  float dbA;
  float dbB;
  float dbC;
  float dbD;
  bool isABRed; // Use a boolean instead of text for efficiency
};

// --- Device Objects ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

RF24 radio(NRF_CE_PIN, NRF_CSN_PIN);
const byte address[6] = "00001";

SoftWire softWire(RTC_SDA_PIN, RTC_SCL_PIN);
RtcDS3231<SoftWire> Rtc(softWire);

// --- Logic Variables ---
const float SOUND_THRESHOLD_DB = 85.0; 
const long INITIAL_RED_TIME = 10000;
const long PENALTY_TIME = 5000;
unsigned long timerEndTime;
bool isABRoadRed = true;

// Forward Declaration
void reportViolationAndSendData(float dbA, float dbB, float dbC, float dbD);

void setup() {
  Serial.begin(9600);
  while (!Serial);
  Serial.println(F("Arduino Uno - Event-Driven Sound Monitor Initialized."));

  // Set pin modes
  pinMode(AB_RED_LED_PIN, OUTPUT);
  pinMode(AB_GREEN_LED_PIN, OUTPUT);
  pinMode(CD_RED_LED_PIN, OUTPUT);
  pinMode(CD_GREEN_LED_PIN, OUTPUT);

  Wire.begin(); 
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.clearDisplay(); display.display(); display.setTextColor(SSD1306_WHITE);

  Rtc.Begin();

  // --- Initialize NRF24L01 Radio (using fire bot method) ---
  radio.begin();
  radio.setPALevel(RF24_PA_LOW);  // Set power level
  radio.setChannel(100);          // Use a non-default channel
  radio.openWritingPipe(address);
  radio.stopListening();
  Serial.println("NRF24L01 Radio Initialized as Transmitter.");

  timerEndTime = millis() + INITIAL_RED_TIME;
  updateLights();
}

void loop() {
  if (millis() >= timerEndTime) {
    isABRoadRed = !isABRoadRed;
    timerEndTime = millis() + INITIAL_RED_TIME;
    updateLights();
    Serial.println(F("\n--- Traffic Light Switched ---"));
  }

  float dbA = map(analogRead(SENSOR_A_PIN), 0, 1023, 30, 120);
  float dbB = map(analogRead(SENSOR_B_PIN), 0, 1023, 30, 120);
  float dbC = map(analogRead(SENSOR_C_PIN), 0, 1023, 30, 120);
  float dbD = map(analogRead(SENSOR_D_PIN), 0, 1023, 30, 120);

  if (isABRoadRed) {
    if (dbA < SOUND_THRESHOLD_DB || dbB < SOUND_THRESHOLD_DB) {
      Serial.println(F("\n>>> SOUND VIOLATION DETECTED ON AB ROAD <<<"));
      reportViolationAndSendData(dbA, dbB, dbC, dbD);
      timerEndTime += PENALTY_TIME;
    }
  } else {
    if (dbC < SOUND_THRESHOLD_DB || dbD < SOUND_THRESHOLD_DB) {
      Serial.println(F("\n>>> SOUND VIOLATION DETECTED ON CD ROAD <<<"));
      reportViolationAndSendData(dbA, dbB, dbC, dbD);
      timerEndTime += PENALTY_TIME;
    }
  }

  updateDisplay();
  delay(100);
}

void reportViolationAndSendData(float dbA, float dbB, float dbC, float dbD) {
  // Step 1: Create an instance of our data structure
  SensorData dataPacket;

  // Step 2: Fill the structure with the current data
  dataPacket.dbA = dbA;
  dataPacket.dbB = dbB;
  dataPacket.dbC = dbC;
  dataPacket.dbD = dbD;
  dataPacket.isABRed = isABRoadRed;
  
  // Step 3: Send the entire struct. This is a fixed size.
  bool report = radio.write(&dataPacket, sizeof(dataPacket));
  
  if(report){
    Serial.println(F("Data packet successfully sent to ESP32."));
  } else {
    Serial.println(F("!!! FAILED to send data packet. !!!"));
  }
}

void updateLights() {
  digitalWrite(AB_RED_LED_PIN, isABRoadRed);
  digitalWrite(AB_GREEN_LED_PIN, !isABRoadRed);
  digitalWrite(CD_RED_LED_PIN, !isABRoadRed);
  digitalWrite(CD_GREEN_LED_PIN, isABRoadRed);
}

void updateDisplay() {
  display.clearDisplay();
  long remainingTime = (timerEndTime - millis()) / 1000;
  if (remainingTime < 0) remainingTime = 0;

  display.setTextSize(3);
  display.setCursor(25, 10);
  
  if (remainingTime < 10) {
    display.print("0");
  }
  display.print(remainingTime);
  
  display.display();
}
