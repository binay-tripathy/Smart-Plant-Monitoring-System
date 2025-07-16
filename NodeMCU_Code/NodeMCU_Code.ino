// Template Configuration 
#define BLYNK_TEMPLATE_ID "TMPL3CiIem3Nu"
#define BLYNK_TEMPLATE_NAME "Quickstart Template"
#define BLYNK_AUTH_TOKEN "NF1EEVKXlDxzoXYgukJXEQpGpA78O9L-"

// Libraries
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <DHT.h>

// Debugging
#define BLYNK_PRINT Serial

// Hardware Pins
#define DHTPIN D3
#define DHTTYPE DHT11
#define RELAY_PIN D5
#define SOIL_SENSOR A0

// Virtual Pins
#define V_TEMP V0
#define V_HUM V1
#define V_SOIL_PERCENT V2  
#define V_PUMP V3
#define V_SYSTEM_ENABLE V4  // System ON/OFF toggle

// Function prototypes
void sendSensorData();
void checkMoisture();
void activatePump(int duration);
int calculateMoisture(int raw);
void checkConnection();

const int AIR_DRY = 715;
const int WATER_WET = 300;

unsigned long lastPumpTime = 0;
const long pumpCooldown = 30000;

unsigned long lastAlertTime = 0;
const long alertCooldown = 60000;

DHT dht(DHTPIN, DHTTYPE);
BlynkTimer timer;

// System Variables
bool systemEnabled = false;
bool pumpRunning = false;
int soilRaw = 0;

// WiFi Credentials
char ssid[] = "Binay";
char pass[] = "Binay@10";

void setup() {
  Serial.begin(115200);

  // Initialize hardware
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  pinMode(DHTPIN, INPUT_PULLUP);
  dht.begin();

  // Connect to Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("Device started");

  // Timers
  timer.setInterval(2000L, sendSensorData);
  timer.setInterval(5000L, checkMoisture);
  timer.setInterval(60000L, checkConnection);

  // Blynk initial state
  Blynk.virtualWrite(V_SOIL_PERCENT, 0);
  Blynk.virtualWrite(V_PUMP, 0);
  Blynk.virtualWrite(V_SYSTEM_ENABLE, 0);
}

void loop() {
  Blynk.run();
  timer.run();
}

void checkConnection() {
  if (!Blynk.connected() || WiFi.status() != WL_CONNECTED) {
    Serial.println("Reconnecting...");
    Blynk.connect();
  }
}

void checkMoisture() {
  if (!systemEnabled) return;

  soilRaw = analogRead(SOIL_SENSOR);
  int moisturePercent = calculateMoisture(soilRaw);
  float h = dht.readHumidity();

  if (moisturePercent < 30 && h < 65 && 
      (millis() - lastPumpTime > pumpCooldown)) {
    activatePump(2000);
    lastPumpTime = millis();
    Serial.println("Pump activated for 2s");
  }

  // Soil moisture alert
  if (moisturePercent < 30 && (millis() - lastAlertTime > alertCooldown)) {
    String message = String(moisturePercent) + "%";
    Blynk.logEvent("low_soil_moisture", message);
    lastAlertTime = millis();
    Serial.println("Soil Moisture Alert Sent: " + message);
  }
}

int calculateMoisture(int raw) {
  int percent = map(raw, AIR_DRY, WATER_WET, 0, 100);
  return constrain(percent, 0, 100);
}

void sendSensorData() {
  if (!systemEnabled) return;

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("DHT11 Error!");
    Blynk.virtualWrite(V_HUM, "Error");
    Blynk.virtualWrite(V_TEMP, "Error");
    return;
  }

  h = constrain(h, 0, 100);
  t = constrain(t, 0, 50);

  // Temperature alerts
  if (t < 16 && (millis() - lastAlertTime > alertCooldown)) {
    String message = String(t, 1) + "°C";
    Blynk.logEvent("low_temperature", message);
    lastAlertTime = millis();
    Serial.println("Low Temp Alert: " + message);
  }

  if (t > 25 && (millis() - lastAlertTime > alertCooldown)) {
    String message = String(t, 1) + "°C";
    Blynk.logEvent("high_temperature", message);
    lastAlertTime = millis();
    Serial.println("High Temp Alert: " + message);
  }

  int moisturePercent = calculateMoisture(analogRead(SOIL_SENSOR));

  Blynk.virtualWrite(V_TEMP, t);
  Blynk.virtualWrite(V_HUM, h);
  Blynk.virtualWrite(V_SOIL_PERCENT, moisturePercent);

  Serial.print("Temp: ");
  Serial.print(t);
  Serial.print("°C\tHumidity: ");
  Serial.print(h);
  Serial.print("%\tSoil Moisture: ");
  Serial.print(moisturePercent);
  Serial.println("%");
}

void activatePump(int duration) {
  if (!systemEnabled) return;

  digitalWrite(RELAY_PIN, HIGH);
  pumpRunning = true;
  Blynk.virtualWrite(V_PUMP, 1);

  timer.setTimeout(duration, []() {
    digitalWrite(RELAY_PIN, LOW);
    pumpRunning = false;
    Blynk.virtualWrite(V_PUMP, 0);
  });
}

// Blynk Handlers
BLYNK_WRITE(V_PUMP) {
  if (systemEnabled && param.asInt() == 1) {
    activatePump(2000);
  } else if (param.asInt() == 0) {
    digitalWrite(RELAY_PIN, LOW);
    pumpRunning = false;
    Blynk.virtualWrite(V_PUMP, 0);
  }
}

BLYNK_WRITE(V_SYSTEM_ENABLE) {
  systemEnabled = param.asInt();

  if (!systemEnabled) {
    digitalWrite(RELAY_PIN, LOW);
    pumpRunning = false;
    Blynk.virtualWrite(V_PUMP, 0);
    Serial.println("System disabled");
  } else {
    Serial.println("System enabled");
  }
}
