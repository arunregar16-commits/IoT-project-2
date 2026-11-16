#define BLYNK_TEMPLATE_ID "TMPLxxxxxx"
#define BLYNK_TEMPLATE_NAME "Smart Plant Monitoring"
#define BLYNK_AUTH_TOKEN "Your_Blynk_Auth_Token"

#define BLYNK_PRINT Serial

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// WiFi Details
char ssid[] = "Your_WiFi_Name";
char pass[] = "Your_WiFi_Password";

// Sensor Pins
#define SOIL_PIN A0
#define DHTPIN D4
#define DHTTYPE DHT11
#define PIR_PIN D6
#define RELAY_PIN D5
#define BUTTON_PIN D7

// Relay is active LOW
#define RELAY_ON LOW
#define RELAY_OFF HIGH

// LCD 20x4
LiquidCrystal_I2C lcd(0x27, 20, 4);

// DHT Sensor
DHT dht(DHTPIN, DHTTYPE);

// Blynk Timer
BlynkTimer timer;

// Soil calibration values
int dryValue = 850;
int wetValue = 350;
int moistureThreshold = 35;

// System variables
int soilMoisture = 0;
int pirState = 0;

float temperature = 0;
float humidity = 0;

bool autoMode = true;
bool manualPump = false;
bool pumpState = false;

bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;

// LCD line function
void lcdLine(int row, String message) {
  lcd.setCursor(0, row);
  if (message.length() > 20)
    message = message.substring(0, 20);

  lcd.print(message);
  for (int i = message.length(); i < 20; i++) {
    lcd.print(" ");
  }
}

// Pump control
void controlPump(bool state) {
  pumpState = state;

  digitalWrite(RELAY_PIN,
               state ? RELAY_ON : RELAY_OFF);

  Blynk.virtualWrite(V6, pumpState);
}

// Read sensors
void readSensors() {

  int soilRaw = analogRead(SOIL_PIN);

  soilMoisture = map(
    soilRaw, dryValue, wetValue, 0, 100
  );

  soilMoisture = constrain(soilMoisture, 0, 100);

  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  pirState = digitalRead(PIR_PIN);

  // Send sensor data to Blynk
  if (Blynk.connected()) {

    Blynk.virtualWrite(V0, soilMoisture);

    if (!isnan(temperature)) {
      Blynk.virtualWrite(V1, temperature);
    }

    if (!isnan(humidity)) {
      Blynk.virtualWrite(V2, humidity);
    }

    Blynk.virtualWrite(V3, pirState);
  }

  // Automatic watering
  if (autoMode) {

    if (soilMoisture < moistureThreshold) {
      controlPump(true);
    } else {
      controlPump(false);
    }

  } else {

    controlPump(manualPump);
  }

  // LCD Display
  lcdLine(0, "SMART PLANT SYSTEM");

  lcdLine(1, "Soil: " +
          String(soilMoisture) + "%");

  if (!isnan(temperature) && !isnan(humidity)) {
    lcdLine(2, "Temp:" +
            String(temperature, 1) +
            "C Hum:" +
            String(humidity, 0) + "%");
  } else {
    lcdLine(2, "DHT11 Sensor Error");
  }

  lcdLine(3, String(autoMode ? "AUTO " : "MANUAL ") +
          "Pump:" +
          String(pumpState ? "ON" : "OFF") +
          " PIR:" +
          String(pirState ? "YES" : "NO"));
}

// Blynk Manual Pump Switch (V4)
BLYNK_WRITE(V4) {

  manualPump = param.asInt();

  if (!autoMode) {
    controlPump(manualPump);
  }
}

// Blynk Automatic Mode Switch (V5)
BLYNK_WRITE(V5) {

  autoMode = param.asInt();

  if (autoMode) {
    if (soilMoisture < moistureThreshold) {
      controlPump(true);
    } else {
      controlPump(false);
    }
  } else {
    controlPump(manualPump);
  }
}

// Blynk Connected
BLYNK_CONNECTED() {
  Blynk.syncVirtual(V4, V5);
}

// Physical Push Button
void checkButton() {

  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > 50) {

    static bool buttonHandled = false;

    if (reading == LOW && !buttonHandled) {

      buttonHandled = true;

      if (!autoMode) {

        manualPump = !manualPump;

        controlPump(manualPump);

        Blynk.virtualWrite(V4, manualPump);
      }
    }

    if (reading == HIGH) {
      buttonHandled = false;
    }
  }

  lastButtonState = reading;
}

void setup() {

  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  digitalWrite(RELAY_PIN, RELAY_OFF);

  // Start LCD
  Wire.begin(D2, D1);
  lcd.init();
  lcd.backlight();

  lcdLine(0, "SMART PLANT SYSTEM");
  lcdLine(1, "Initializing...");
  lcdLine(2, "Connecting WiFi");
  lcdLine(3, "Please Wait");

  dht.begin();

  // Connect to Blynk
  Blynk.begin(
    BLYNK_AUTH_TOKEN,
    ssid,
    pass
  );

  // Sensor reading every 2 seconds
  timer.setInterval(2000L, readSensors);
}

void loop() {

  Blynk.run();
  timer.run();

  checkButton();
}
