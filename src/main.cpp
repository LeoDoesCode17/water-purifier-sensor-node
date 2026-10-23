#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ---------------- Pin configuration ----------------
#define PIN_PH        A0
#define PIN_TURBIDITY A2
#define PIN_TDS       A4
#define PIN_DS18B20   2    // 4.7k pull-up to 5V
#define PIN_TRIG      9
#define PIN_ECHO      10

// ---------------- Settings ----------------
#define VREF            5.0f     // ADC reference (Uno powered via USB/5V)
#define ADC_RES         1024.0f
#define SAMPLE_COUNT    15       // samples per reading (median filter)
#define SAMPLE_DELAY_MS 10
#define REPORT_MS       2000

// pH calibration (two-point). Measure the sensor voltage in pH 7.00 and
// pH 4.01 buffer solutions and put the values here.
#define PH7_VOLTAGE     2.50f
#define PH4_VOLTAGE     3.05f

// Tank geometry for ultrasonic water level
#define SENSOR_TO_BOTTOM_CM 30.0f   // distance from sensor face to tank bottom

OneWire oneWire(PIN_DS18B20);
DallasTemperature ds18b20(&oneWire);

// ---------------- Helpers ----------------
// Read analog pin SAMPLE_COUNT times and return the median (noise rejection)
float readVoltageMedian(uint8_t pin) {
  int buf[SAMPLE_COUNT];
  for (uint8_t i = 0; i < SAMPLE_COUNT; i++) {
    buf[i] = analogRead(pin);
    delay(SAMPLE_DELAY_MS);
  }
  // insertion sort
  for (uint8_t i = 1; i < SAMPLE_COUNT; i++) {
    int key = buf[i];
    int j = i - 1;
    while (j >= 0 && buf[j] > key) {
      buf[j + 1] = buf[j];
      j--;
    }
    buf[j + 1] = key;
  }
  return buf[SAMPLE_COUNT / 2] * VREF / ADC_RES;
}

float readPH(float &voltage) {
  voltage = readVoltageMedian(PIN_PH);
  // Linear map through the two calibration points
  float slope = (7.0f - 4.01f) / (PH7_VOLTAGE - PH4_VOLTAGE);
  return 7.0f + slope * (voltage - PH7_VOLTAGE);
}

float readTurbidityNTU(float &voltage) {
  voltage = readVoltageMedian(PIN_TURBIDITY);
  // Common polynomial for the SEN0189-style sensor; clean water ~4.1-4.2V
  float ntu;
  if (voltage < 2.5f) {
    ntu = 3000.0f;
  } else {
    ntu = -1120.4f * voltage * voltage + 5742.3f * voltage - 4352.9f;
    if (ntu < 0) ntu = 0;
  }
  return ntu;
}

float readTDS(float tempC, float &voltage) {
  voltage = readVoltageMedian(PIN_TDS);
  // Temperature compensation (reference 25 C)
  float coeff = 1.0f + 0.02f * (tempC - 25.0f);
  float v = voltage / coeff;
  float tds = (133.42f * v * v * v - 255.86f * v * v + 857.39f * v) * 0.5f;
  return tds < 0 ? 0 : tds;
}

float readDistanceCm() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  unsigned long dur = pulseIn(PIN_ECHO, HIGH, 30000UL);  // 30 ms timeout (~5 m)
  if (dur == 0) return -1.0f;
  return dur * 0.0343f / 2.0f;
}

// ---------------- Arduino ----------------
void setup() {
  Serial.begin(9600);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  ds18b20.begin();
  Serial.println(F("Water Quality Monitor - Arduino Uno R3"));

}

void loop() {
  static unsigned long last = 0;
  if (millis() - last < REPORT_MS) return;
  last = millis();

  ds18b20.requestTemperatures();
  float tempC = ds18b20.getTempCByIndex(0);
  if (tempC == DEVICE_DISCONNECTED_C) {
    tempC = 25.0f;  // fallback so TDS compensation still works
    Serial.println(F("WARN: DS18B20 not found, using 25C"));
  }

  float phV, turbV, tdsV;
  float ph   = readPH(phV);
  float turb = readTurbidityNTU(turbV);
  float tds  = readTDS(tempC, tdsV);
  float dist = readDistanceCm();
  float level = (dist < 0) ? -1.0f : SENSOR_TO_BOTTOM_CM - dist;
  if (level < 0 && dist >= 0) level = 0;

  Serial.println(F("=============================="));

  Serial.print(F("TEMPERATURE    : "));
  Serial.print(tempC, 2);
  Serial.println(F(" C"));

  Serial.print(F("PH VALUE       : "));
  Serial.print(ph, 2);
  Serial.print(F("  ("));
  Serial.print(phV, 3);
  Serial.println(F(" V)"));

  Serial.print(F("TURBIDITY      : "));
  Serial.print(turb, 1);
  Serial.print(F(" NTU  ("));
  Serial.print(turbV, 3);
  Serial.println(F(" V)"));

  Serial.print(F("TDS VALUE      : "));
  Serial.print(tds, 0);
  Serial.print(F(" ppm  ("));
  Serial.print(tdsV, 3);
  Serial.println(F(" V)"));

  Serial.print(F("WATER DISTANCE : "));
  if (dist < 0) {
    Serial.println(F("OUT OF RANGE"));
  } else {
    Serial.print(dist, 1);
    Serial.println(F(" cm"));
  }

  Serial.print(F("WATER LEVEL    : "));
  if (dist < 0) {
    Serial.println(F("OUT OF RANGE"));
  } else {
    Serial.print(level, 1);
    Serial.println(F(" cm"));
  }

  Serial.println();
}