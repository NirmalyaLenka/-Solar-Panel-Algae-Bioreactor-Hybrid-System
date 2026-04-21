/*
 * Solar-Algae Hybrid System — ESP32 Firmware
 * ============================================
 * Monitors: Temperature, pH, Light (lux), CO₂, Power (V/A/W)
 * Controls: Air pump, Water pump, LED grow lights (via relay)
 * Reports: Serial + WiFi MQTT dashboard
 *
 * Board: ESP32 DevKit V1
 * Framework: Arduino (PlatformIO)
 *
 * Author: Solar-Algae Project Contributors
 * License: MIT
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>       // MQTT
#include <OneWire.h>
#include <DallasTemperature.h>  // DS18B20
#include <Wire.h>
#include <BH1750.h>             // Light sensor
#include <Adafruit_INA219.h>    // Power monitor
#include <MHZ19.h>              // CO2 sensor
#include <ArduinoJson.h>

// ─── WiFi & MQTT Config ─────────────────────────────────────────────
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* MQTT_SERVER   = "192.168.1.100";  // Your MQTT broker IP
const int   MQTT_PORT     = 1883;
const char* MQTT_TOPIC    = "solar_algae/sensors";
const char* MQTT_CMD      = "solar_algae/control";

// ─── GPIO Pin Definitions ────────────────────────────────────────────
#define ONE_WIRE_BUS    4      // DS18B20 temperature sensor
#define PH_SENSOR_PIN   34     // pH analog input (ADC)
#define CO2_RX_PIN      16     // MH-Z19B RX
#define CO2_TX_PIN      17     // MH-Z19B TX
#define RELAY_AIR_PUMP  25     // Relay 1: Air pump (LOW = ON)
#define RELAY_WATER_PUMP 26    // Relay 2: Water circulation pump
#define RELAY_GROW_LED  27     // Relay 3: Supplemental LED lights

// ─── Timing ─────────────────────────────────────────────────────────
#define SENSOR_INTERVAL   10000   // Read sensors every 10 seconds
#define PUMP_ON_INTERVAL  1800000 // Air pump ON 30 min of every hour
#define PUMP_OFF_INTERVAL 1800000 // Air pump OFF 30 min

// ─── Algae Culture Thresholds ────────────────────────────────────────
#define PH_MIN        6.5
#define PH_MAX        8.5
#define TEMP_MIN      18.0   // °C
#define TEMP_MAX      35.0   // °C
#define CO2_HIGH_PPM  2000   // Trigger ventilation alert
#define LUX_MIN       1000   // Below this = turn on grow LEDs

// ─── Object Instantiation ────────────────────────────────────────────
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensor(&oneWire);
BH1750 lightMeter;
Adafruit_INA219 powerMonitor;
HardwareSerial co2Serial(1);
MHZ19 co2Sensor;
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ─── State Variables ─────────────────────────────────────────────────
unsigned long lastSensorRead = 0;
unsigned long pumpCycleStart = 0;
bool airPumpState = false;
bool waterPumpState = true;  // Water pump runs continuously
bool growLEDState = false;

// ─── Sensor Data Struct ──────────────────────────────────────────────
struct SensorData {
  float temperature;   // °C
  float pH;
  float lux;
  int   co2_ppm;
  float voltage;       // Battery/panel voltage
  float current;       // Current draw (A)
  float power;         // Watts
};

// ─── Function Prototypes ─────────────────────────────────────────────
void setupWiFi();
void setupMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void reconnectMQTT();
SensorData readAllSensors();
float readPH();
void publishData(SensorData& data);
void controlPumps(SensorData& data);
void printSerial(SensorData& data);
String buildAlertMessage(SensorData& data);

// ─── Setup ───────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n🌿☀️  Solar-Algae Hybrid System Starting...");

  // Relay pins (HIGH = OFF for active-low relays)
  pinMode(RELAY_AIR_PUMP,   OUTPUT); digitalWrite(RELAY_AIR_PUMP,   HIGH);
  pinMode(RELAY_WATER_PUMP, OUTPUT); digitalWrite(RELAY_WATER_PUMP, HIGH);
  pinMode(RELAY_GROW_LED,   OUTPUT); digitalWrite(RELAY_GROW_LED,   HIGH);

  // Sensor init
  Wire.begin();
  tempSensor.begin();
  
  if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println("⚠️  BH1750 light sensor not found!");
  }
  
  if (!powerMonitor.begin()) {
    Serial.println("⚠️  INA219 power monitor not found!");
  }

  co2Serial.begin(9600, SERIAL_8N1, CO2_RX_PIN, CO2_TX_PIN);
  co2Sensor.begin(co2Serial);
  co2Sensor.autoCalibration(false);

  // Network
  setupWiFi();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  // Turn on water pump immediately
  digitalWrite(RELAY_WATER_PUMP, LOW);
  waterPumpState = true;
  
  pumpCycleStart = millis();
  Serial.println("✅ System initialized. Monitoring started.");
}

// ─── Main Loop ───────────────────────────────────────────────────────
void loop() {
  // MQTT keep-alive
  if (!mqttClient.connected()) reconnectMQTT();
  mqttClient.loop();

  unsigned long now = millis();

  // Sensor read & publish
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;
    SensorData data = readAllSensors();
    printSerial(data);
    publishData(data);
    controlPumps(data);
  }

  // Air pump duty cycle (30 min ON / 30 min OFF)
  if (now - pumpCycleStart < PUMP_ON_INTERVAL) {
    if (!airPumpState) {
      digitalWrite(RELAY_AIR_PUMP, LOW);
      airPumpState = true;
      Serial.println("🌬️  Air pump ON");
    }
  } else if (now - pumpCycleStart < PUMP_ON_INTERVAL + PUMP_OFF_INTERVAL) {
    if (airPumpState) {
      digitalWrite(RELAY_AIR_PUMP, HIGH);
      airPumpState = false;
      Serial.println("💤 Air pump OFF");
    }
  } else {
    pumpCycleStart = now;
  }
}

// ─── Read All Sensors ────────────────────────────────────────────────
SensorData readAllSensors() {
  SensorData d;

  // Temperature
  tempSensor.requestTemperatures();
  d.temperature = tempSensor.getTempCByIndex(0);
  if (d.temperature == DEVICE_DISCONNECTED_C) d.temperature = -1;

  // pH
  d.pH = readPH();

  // Light
  d.lux = lightMeter.readLightLevel();

  // CO₂
  d.co2_ppm = co2Sensor.getCO2();

  // Power monitoring
  d.voltage = powerMonitor.getBusVoltage_V() + (powerMonitor.getShuntVoltage_mV() / 1000.0);
  d.current = powerMonitor.getCurrent_mA() / 1000.0;
  d.power   = d.voltage * d.current;

  return d;
}

// ─── pH Sensor Reading ───────────────────────────────────────────────
float readPH() {
  // Average 10 ADC readings for stability
  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(PH_SENSOR_PIN);
    delay(10);
  }
  float avgADC = sum / 10.0;
  
  // Convert ADC to voltage (ESP32 ADC is 12-bit, 3.3V ref)
  float voltage = avgADC * (3.3 / 4095.0);
  
  // Voltage divider corrects 5V sensor to 3.3V range
  // Actual sensor voltage = measured × (30k/20k) = × 1.5
  float sensorVoltage = voltage * 1.5;
  
  // Typical pH sensor calibration: pH = -5.70 × V + 21.34
  // Calibrate with buffer solutions (pH 4.0 and 7.0)
  float pH = -5.70f * sensorVoltage + 21.34f;
  
  return constrain(pH, 0.0f, 14.0f);
}

// ─── Pump / LED Control Logic ────────────────────────────────────────
void controlPumps(SensorData& data) {
  // Auto grow LED: turn on if light is too low
  bool needLED = (data.lux < LUX_MIN);
  if (needLED != growLEDState) {
    digitalWrite(RELAY_GROW_LED, needLED ? LOW : HIGH);
    growLEDState = needLED;
    Serial.printf("💡 Grow LED: %s (Lux: %.0f)\n", needLED ? "ON" : "OFF", data.lux);
  }

  // Alert logic (print warnings — add buzzer/SMS as needed)
  if (data.pH < PH_MIN)   Serial.printf("⚠️  pH LOW: %.2f (min %.1f)\n", data.pH, PH_MIN);
  if (data.pH > PH_MAX)   Serial.printf("⚠️  pH HIGH: %.2f (max %.1f)\n", data.pH, PH_MAX);
  if (data.temperature > TEMP_MAX) Serial.printf("🌡️  TEMP HIGH: %.1f°C\n", data.temperature);
  if (data.temperature < TEMP_MIN && data.temperature > 0)
                          Serial.printf("🌡️  TEMP LOW: %.1f°C\n", data.temperature);
  if (data.co2_ppm > CO2_HIGH_PPM) Serial.printf("⚠️  CO₂ HIGH: %d ppm\n", data.co2_ppm);
}

// ─── MQTT Publish ────────────────────────────────────────────────────
void publishData(SensorData& data) {
  if (!mqttClient.connected()) return;

  StaticJsonDocument<256> doc;
  doc["temp_c"]    = round(data.temperature * 10) / 10.0;
  doc["pH"]        = round(data.pH * 100) / 100.0;
  doc["lux"]       = round(data.lux);
  doc["co2_ppm"]   = data.co2_ppm;
  doc["voltage_v"] = round(data.voltage * 100) / 100.0;
  doc["current_a"] = round(data.current * 1000) / 1000.0;
  doc["power_w"]   = round(data.power * 100) / 100.0;
  doc["air_pump"]  = airPumpState;
  doc["grow_led"]  = growLEDState;
  doc["uptime_s"]  = millis() / 1000;

  char buffer[256];
  serializeJson(doc, buffer);
  mqttClient.publish(MQTT_TOPIC, buffer);
}

// ─── Serial Print ────────────────────────────────────────────────────
void printSerial(SensorData& data) {
  Serial.println("─────────────────────────────");
  Serial.printf("🌡️  Temp:    %.1f °C\n",      data.temperature);
  Serial.printf("🧪 pH:      %.2f\n",           data.pH);
  Serial.printf("💡 Light:   %.0f lux\n",       data.lux);
  Serial.printf("💨 CO₂:     %d ppm\n",         data.co2_ppm);
  Serial.printf("⚡ Voltage: %.2f V\n",          data.voltage);
  Serial.printf("⚡ Current: %.3f A\n",          data.current);
  Serial.printf("⚡ Power:   %.2f W\n",          data.power);
}

// ─── WiFi Setup ──────────────────────────────────────────────────────
void setupWiFi() {
  Serial.printf("Connecting to WiFi: %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(500);
    Serial.print(".");
    tries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n✅ WiFi connected: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n⚠️  WiFi failed — running in offline mode.");
  }
}

// ─── MQTT Reconnect ──────────────────────────────────────────────────
void reconnectMQTT() {
  int tries = 0;
  while (!mqttClient.connected() && tries < 3) {
    if (mqttClient.connect("SolarAlgaeSystem")) {
      mqttClient.subscribe(MQTT_CMD);
      Serial.println("✅ MQTT connected");
    } else {
      tries++;
      delay(2000);
    }
  }
}

// ─── MQTT Command Callback ───────────────────────────────────────────
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  
  Serial.printf("📩 MQTT command: %s\n", msg.c_str());

  if (msg == "air_pump_on")    { digitalWrite(RELAY_AIR_PUMP, LOW);  airPumpState = true; }
  if (msg == "air_pump_off")   { digitalWrite(RELAY_AIR_PUMP, HIGH); airPumpState = false; }
  if (msg == "water_pump_on")  { digitalWrite(RELAY_WATER_PUMP, LOW);  waterPumpState = true; }
  if (msg == "water_pump_off") { digitalWrite(RELAY_WATER_PUMP, HIGH); waterPumpState = false; }
  if (msg == "led_on")         { digitalWrite(RELAY_GROW_LED, LOW);  growLEDState = true; }
  if (msg == "led_off")        { digitalWrite(RELAY_GROW_LED, HIGH); growLEDState = false; }
}
