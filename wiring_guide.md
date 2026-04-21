# Firmware Wiring Guide

## ESP32 to Sensor Connections

### DS18B20 Temperature Sensor
```
DS18B20 VCC  → ESP32 3.3V
DS18B20 GND  → ESP32 GND
DS18B20 DATA → ESP32 GPIO 4 + 4.7kΩ pull-up to 3.3V
```

### BH1750 Light Sensor (I²C)
```
BH1750 VCC  → ESP32 3.3V
BH1750 GND  → ESP32 GND
BH1750 SDA  → ESP32 GPIO 21
BH1750 SCL  → ESP32 GPIO 22
BH1750 ADDR → GND (sets I²C address to 0x23)
```

### INA219 Power Monitor (I²C, shared bus)
```
INA219 VCC  → ESP32 3.3V
INA219 GND  → ESP32 GND
INA219 SDA  → ESP32 GPIO 21  (same I²C bus as BH1750)
INA219 SCL  → ESP32 GPIO 22
INA219 VIN+ → Battery positive (before load)
INA219 VIN- → Battery positive (after shunt, to load)
```
> The INA219 uses a 0.1Ω shunt resistor on the VIN+/VIN- path to measure current.

### MH-Z19B CO₂ Sensor (UART)
```
MH-Z19B VCC → 5V (requires 5V!)
MH-Z19B GND → GND
MH-Z19B TX  → ESP32 GPIO 16 (ESP32 RX)
MH-Z19B RX  → ESP32 GPIO 17 (ESP32 TX) via 1kΩ resistor (5V→3.3V protection)
```
> Use a 1kΩ resistor in series on the TX→RX line to protect the 3.3V ESP32 input.

### pH Sensor (Analog)
```
pH Board VCC → 5V
pH Board GND → GND
pH Board OUT → Voltage divider → ESP32 GPIO 34

Voltage divider:
OUT ─[10kΩ]─┬─[20kΩ]─ GND
             └────────► GPIO 34
```

### 4-Channel Relay Module
```
Relay VCC → 5V
Relay GND → GND
Relay IN1 → ESP32 GPIO 25  (Air pump)
Relay IN2 → ESP32 GPIO 26  (Water pump)
Relay IN3 → ESP32 GPIO 27  (Grow LEDs)
Relay IN4 → (spare)

Relay COM1 → 12V+
Relay NO1  → Air pump +
(Air pump -  → 12V GND)

Same pattern for relay 2 and 3.
```

## Calibrating the pH Sensor

1. Prepare pH 4.0 and pH 7.0 buffer solutions (available at aquarium shops).
2. Open `Serial Monitor` at 115200 baud.
3. Place electrode in pH 7.0 buffer and note the `sensorVoltage` output.
4. Place electrode in pH 4.0 buffer and note the voltage.
5. Calculate slope = (7.0 - 4.0) / (V7 - V4)
6. Calculate intercept = 7.0 - slope × V7
7. Update the formula in `readPH()` in `main.cpp`:
   ```cpp
   float pH = slope * sensorVoltage + intercept;
   ```

## Calibrating the CO₂ Sensor (MH-Z19B)

The MH-Z19B has built-in auto-calibration (ABC logic) that assumes the lowest reading
in a 24-hour period equals 400 ppm (outdoor fresh air).

- If installed indoors: disable auto-calibration: `co2Sensor.autoCalibration(false);`
- Manual zero calibration: expose to outdoor air for 20+ minutes, then call `co2Sensor.calibrateZero();`
