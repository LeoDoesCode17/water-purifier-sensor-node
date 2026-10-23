# Water Quality Monitoring System (Arduino Uno R3)

A water quality monitoring system built on the Arduino Uno R3. It measures **two sets** of water parameters (two pH, two turbidity, two TDS, two temperature sensors) plus water level via an ultrasonic sensor.

## Hardware

| Qty | Component | Purpose |
|---|---|---|
| 1 | Arduino Uno R3 | Main controller |
| 2 | Analog pH sensor module (e.g. PH-4502C) | Acidity / alkalinity |
| 2 | Analog turbidity sensor module | Water clarity |
| 2 | Analog TDS sensor module (e.g. Gravity TDS) | Total dissolved solids |
| 1 | HC-SR04 ultrasonic sensor | Water level / distance |
| 2 | DS18B20 waterproof temperature probe | Temperature (two measurement points) |
| 1 | 4.7 kΩ resistor | Shared 1-Wire pull-up for both DS18B20 |
| - | Breadboard + jumper wires | Connections |

## Sensor Sets

| | Set 1 | Set 2 |
|---|---|---|
| pH | A0 | A3 |
| Turbidity | A1 | A4 |
| TDS | A2 | A5 |
| Temperature (DS18B20) | D2 (shared bus, address #1) | D2 (shared bus, address #2) |

## Pin Configuration

| Sensor | Sensor Pin | Arduino Uno Pin |
|---|---|---|
| **pH sensor #1** | V+ | 5V |
| | G | GND |
| | Po (analog out) | **A0** |
| **Turbidity sensor #1** | VCC | 5V |
| | GND | GND |
| | Signal (A) | **A2** |
| **TDS sensor #1** | VCC | 5V |
| | GND | GND |
| | Signal (A) | **A4** |
| **pH sensor #2** | V+ | 5V |
| | G | GND |
| | Po (analog out) | **A1** |
| **Turbidity sensor #2** | VCC | 5V |
| | GND | GND |
| | Signal (A) | **A3** |
| **TDS sensor #2** | VCC | 5V |
| | GND | GND |
| | Signal (A) | **A5** |
| **DS18B20 #1** | Red (VCC) | 5V |
| | Black (GND) | GND |
| | Yellow/White (DATA) | **D2** (shared bus) |
| **DS18B20 #2** | Red (VCC) | 5V |
| | Black (GND) | GND |
| | Yellow/White (DATA) | **D2** (shared bus) |
| **HC-SR04** | VCC | 5V |
| | GND | GND |
| | Trig | **D9** |
| | Echo | **D10** |

### Pin summary

| Arduino Pin | Used by |
|---|---|
| A0 | pH sensor #1 |
| A1 | Turbidity sensor #1 |
| A2 | TDS sensor #1 |
| A3 | pH sensor #2 |
| A4 | Turbidity sensor #2 |
| A5 | TDS sensor #2 |
| D2 | DS18B20 #1 and #2 (1-Wire bus) |
| D9 | HC-SR04 Trig |
| D10 | HC-SR04 Echo |
| 5V / GND | Power for all sensors |

## Wiring: Two DS18B20 on One Bus

Both DS18B20 sensors share the same data line. Each sensor has a unique 64-bit address, so the Arduino can read them individually. Only **one** 4.7 kΩ pull-up resistor is needed for the whole bus.

```
                 5V
                  |
               [4.7kΩ]
                  |
 D2 --------------+------------- DATA (DS18B20 #1)
                  |
                  +------------- DATA (DS18B20 #2)

 5V  ------------------------- VCC (both sensors)
 GND ------------------------- GND (both sensors)
```

## Wiring Notes

- **All six analog pins are used (A0-A5).** Because A4 and A5 are also the I2C pins (SDA/SCL), you cannot use an I2C device (for example an I2C LCD) on this setup. Use a parallel LCD on the free digital pins, or add an external ADC such as the ADS1115 on I2C to free up analog inputs.
- **Pull-up resistor:** the 4.7 kΩ resistor goes between D2 (DATA) and 5V. Without it the DS18B20 sensors will not be detected.
- **Common ground:** all sensor GNDs must connect to the Arduino GND.
- **Turbidity sensors:** connect the signal from the small control board, not directly from the probe. Its 0-4.5 V output is safe for the Uno's 5V ADC.
- **pH and TDS interference:** probes in the same water can disturb each other's readings (ground loop through the water). This matters more now that there are two pH and two TDS probes. If values are unstable, read the sensors one at a time (for example power each TDS sensor from a digital pin and switch it off while reading pH), or use isolated pH modules. If Set 1 and Set 2 sit in separate water samples, interference is much lower.
- **Power:** twelve-plus modules on the 5V rail add up (turbidity modules draw the most). Use a stable 5V source (USB or a 7-12 V adapter on the barrel jack), and add a 100 µF capacitor across 5V/GND near the sensors if readings are noisy.
- **Free pins:** D3-D8, D11-D13 remain available (for example for a parallel LCD, relay, buzzer, or power-switching the sensors). D10-D13 are SPI, so an SD card module would require moving the HC-SR04 Echo pin.

## Libraries

Install via the Arduino Library Manager:

- `OneWire`
- `DallasTemperature`
- `NewPing` (optional, for the HC-SR04)

## Finding the DS18B20 Addresses

With two sensors on one bus, you can read them by index (`getTempCByIndex(0)` and `getTempCByIndex(1)`), but the order may change if you swap or rewire sensors. For a stable assignment, find each sensor's address once and hard-code it:

```cpp
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 2

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(9600);
  sensors.begin();

  Serial.print("Sensors found: ");
  Serial.println(sensors.getDeviceCount());

  DeviceAddress addr;
  for (uint8_t i = 0; i < sensors.getDeviceCount(); i++) {
    if (sensors.getAddress(addr, i)) {
      Serial.print("Sensor ");
      Serial.print(i);
      Serial.print(" address: ");
      for (uint8_t j = 0; j < 8; j++) {
        if (addr[j] < 16) Serial.print("0");
        Serial.print(addr[j], HEX);
      }
      Serial.println();
    }
  }
}

void loop() {}
```

Label each probe (for example with tape) after identifying it, then use the addresses in your main sketch with `sensors.getTempC(address)`.

## Calibration

Each sensor must be calibrated individually, even if two are the same model, because their readings differ slightly.

- **pH (both):** calibrate each probe with pH 4.0 and pH 6.86 (or 7.0) buffer solutions.
- **TDS (both):** calibrate each with a standard solution (for example 342 ppm or 1413 µS/cm) and apply temperature compensation using the DS18B20 reading from the same set.
- **Turbidity (both):** record the ADC value in clear water as the baseline for each sensor, then compare against known turbid samples.
- **Ultrasonic:** measure the distance from the sensor to the empty-tank bottom and full-tank water surface to convert distance into water level.

## Project Structure (suggested)

```
water-quality-monitor/
├── README.md
└── water_quality_monitor.ino
```