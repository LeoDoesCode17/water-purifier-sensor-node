#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>

typedef struct
{
  float ph_voltage;
  float tds_voltage;
  float turbidity_voltage;
  float temperature_celcius;
  float distance_cm;
} sensor_t;

// ---------------- Pin configuration ----------------
#define PIN_PH A0
#define PIN_TURBIDITY A2
#define PIN_TDS A4
#define PIN_DS18B20 2 // 4.7k pull-up to 5V
#define PIN_TRIG 9
#define PIN_ECHO 10
#define LINK_RX 5 // to ESP32-S3 GPIO 11 (TX1)
#define LINK_TX 4 // to ESP32-S3 GPIO 10 (RX1) via divider

// ---------------- Settings ----------------
#define VREF 5.0f // ADC reference (Uno powered via USB/5V)
#define ADC_RES 1024.0f
#define SAMPLE_COUNT 15 // samples per reading (median filter)
#define SAMPLE_DELAY_MS 10
#define REPORT_MS 2000

// pH calibration (two-point). Measure the sensor voltage in pH 7.00 and
// pH 4.01 buffer solutions and put the values here.
#define PH7_VOLTAGE 2.50f
#define PH4_VOLTAGE 3.05f

// Tank geometry for ultrasonic water level
#define SENSOR_TO_BOTTOM_CM 30.0f // distance from sensor face to tank bottom

OneWire oneWire(PIN_DS18B20);
DallasTemperature ds18b20(&oneWire);
SoftwareSerial link(LINK_RX, LINK_TX);
static sensor_t global_sensor_data;

// ---------------- Helpers ----------------
// Read analog pin SAMPLE_COUNT times and return the median (noise rejection)
float readVoltageMedian(uint8_t pin)
{
  int buf[SAMPLE_COUNT];
  for (uint8_t i = 0; i < SAMPLE_COUNT; i++)
  {
    buf[i] = analogRead(pin);
    delay(SAMPLE_DELAY_MS);
  }
  // insertion sort
  for (uint8_t i = 1; i < SAMPLE_COUNT; i++)
  {
    int key = buf[i];
    int j = i - 1;
    while (j >= 0 && buf[j] > key)
    {
      buf[j + 1] = buf[j];
      j--;
    }
    buf[j + 1] = key;
  }
  return buf[SAMPLE_COUNT / 2] * VREF / ADC_RES;
}

float readDistanceCm()
{
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  unsigned long dur = pulseIn(PIN_ECHO, HIGH, 30000UL); // 30 ms timeout (~5 m)
  if (dur == 0)
    return -1.0f;
  return dur * 0.0343f / 2.0f;
}

void sendMessage(sensor_t &msg)
{
  link.print(F("{"));
  link.print(F("\"ph_voltage\":"));
  link.print(msg.ph_voltage, 2);
  link.print(F(",\"tds_voltage\":"));
  link.print(msg.tds_voltage, 2);
  link.print(F(",\"turbidity_voltage\":"));
  link.print(msg.turbidity_voltage, 2);
  link.print(F(",\"temperature_celcius\":"));
  link.print(msg.temperature_celcius, 2);
  link.print(F(",\"distance_cm\":"));
  link.print(msg.distance_cm, 1);
  link.println(F("}"));
}

// ---------------- Arduino ----------------
void setup()
{
  Serial.begin(9600);
  link.begin(9600);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  ds18b20.begin();
  Serial.println(F("Water Quality Monitor - Arduino Uno R3"));
}

void loop()
{
  static unsigned long last = 0;
  if (millis() - last < REPORT_MS)
    return;
  last = millis();

  ds18b20.requestTemperatures();
  float tempC = ds18b20.getTempCByIndex(0);
  if (tempC == DEVICE_DISCONNECTED_C)
  {
    tempC = 25.0f; // fallback so TDS compensation still works
    Serial.println(F("WARN: DS18B20 not found, using 25C"));
  }

  global_sensor_data.temperature_celcius = tempC;
  global_sensor_data.ph_voltage = readVoltageMedian(PIN_PH);
  global_sensor_data.turbidity_voltage = readVoltageMedian(PIN_TURBIDITY);
  global_sensor_data.tds_voltage = readVoltageMedian(PIN_TDS);
  global_sensor_data.distance_cm = readDistanceCm();

  sendMessage(global_sensor_data);

  Serial.println(F("=============================="));

  Serial.print(F("TEMPERATURE    : "));
  Serial.print(tempC, 2);
  Serial.println(F(" C"));

  Serial.print(F("VOLTAGE PH VALUE       : "));
  Serial.print(global_sensor_data.ph_voltage);
  Serial.println(" V");

  Serial.print(F("VOLTAGE TURBIDITY VALUE : "));
  Serial.print(global_sensor_data.turbidity_voltage);
  Serial.println(" V");

  Serial.print(F("VOLTAGE TDS VALUE : "));
  Serial.print(global_sensor_data.tds_voltage);
  Serial.println(" V");

  Serial.print(F("WATER DISTANCE : "));
  if (global_sensor_data.distance_cm < 0)
  {
    Serial.println(F("OUT OF RANGE"));
  }
  else
  {
    Serial.print(global_sensor_data.distance_cm, 1);
    Serial.println(F(" cm"));
  }

  Serial.println();
}