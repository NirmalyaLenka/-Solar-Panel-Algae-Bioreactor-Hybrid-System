# Circuit Diagram — Solar-Algae Hybrid System

## Full System Wiring Diagram (ASCII)

```
═══════════════════════════════════════════════════════════════════════
                    SOLAR-ALGAE HYBRID SYSTEM
                     Complete Circuit Diagram
═══════════════════════════════════════════════════════════════════════

                     ☀️  SUNLIGHT
                          │
                ┌─────────▼──────────┐
                │  SEMI-TRANSPARENT  │
                │   SOLAR PANEL      │
                │   100W, 18–21V Voc │
                │   ~20% transparent │
                └────┬──────────┬────┘
                   (+)          (-)
                     │          │
                     │          │        (behind panel ↓)
                     │    ┌─────▼──────────────────────┐
                     │    │   ALGAE PHOTOBIOREACTOR     │
                     │    │   (receives IR + diffused   │
                     │    │    light through panel)     │
                     │    └────────────────────────────┘
                     │
          ┌──────────▼──────────────┐
          │      MPPT CHARGE        │
          │      CONTROLLER         │
          │   (e.g. Renogy Rover    │
          │    20A, 12/24V auto)    │
          │                         │
          │  PV+ ────── BAT+ ─────► │──┐
          │  PV- ────── BAT- ─────► │  │
          │             LOAD+/LOAD- │  │
          └──────────┬──────────────┘  │
                     │                 │
          ┌──────────▼──────────┐      │
          │   12V LiFePO4       │◄─────┘
          │   BATTERY PACK      │
          │   (50–100Ah)        │
          │                     │
          │  + ──────────┬──────┘
          │  - ───┐      │
          └───────│──────│──────┘
                  │      │
                  │   ┌──▼──────────────────────────────────┐
                  │   │         POWER DISTRIBUTION BUS       │
                  │   │              12V DC                  │
                  │   └──┬──────┬──────┬──────┬─────────────┘
                  │      │      │      │      │
                  │  ┌───▼──┐ ┌─▼───┐ ┌▼───┐ ┌▼──────────┐
                  │  │RELAY │ │RELAY│ │MPPT│ │ ESP32/     │
                  │  │  1   │ │  2  │ │Load│ │ Arduino    │
                  │  │Air   │ │Water│ │    │ │ Controller │
                  │  │Pump  │ │Pump │ │    │ └─────┬──────┘
                  │  └──┬───┘ └──┬──┘ └────┘       │
                  │     │        │            ┌─────▼──────────────┐
                  │  ┌──▼──┐  ┌──▼──┐        │    SENSORS         │
                  │  │ AIR │  │WATER│        │                    │
                  │  │PUMP │  │PUMP │        │ DS18B20 (Temp)     │
                  │  │12V  │  │12V  │        │ pH Sensor          │
                  │  └──┬──┘  └──┬──┘        │ BH1750 (Light)     │
                  │     │        │            │ MH-Z19B (CO₂)      │
                  │     ▼        ▼            │ INA219 (Power Mon.)│
                  │  [Algae  [Algae           └─────┬──────────────┘
                  │   PBR    PBR                    │
                  │   Air]   Water]                 ▼
                  │                          ┌──────────────┐
                  │                          │  WiFi/BT     │
                  │                          │  Dashboard   │
                  │                          │  (MQTT /     │
                  │                          │   Web UI)    │
                  │                          └──────────────┘
                  │
          ┌───────▼──────────────────┐
          │  THERMOELECTRIC MODULE   │
          │  (Optional: TEG)         │
          │  Hot side → Panel back   │
          │  Cold side → Ambient air │
          │  Generates 0.5–2W extra  │
          └──────────────────────────┘

═══════════════════════════════════════════════════════════════════════
```

---

## ESP32 Pin Mapping

```
ESP32 DevKit V1
─────────────────────────────────────────────
PIN       │ CONNECTED TO          │ TYPE
──────────┼───────────────────────┼──────────
GPIO 4    │ DS18B20 Data (Temp)   │ Digital I/O (OneWire)
GPIO 21   │ BH1750 SDA (Light)    │ I²C SDA
GPIO 22   │ BH1750 SCL (Light)    │ I²C SCL
GPIO 16   │ MH-Z19B RX (CO₂)     │ UART RX
GPIO 17   │ MH-Z19B TX (CO₂)     │ UART TX
GPIO 34   │ pH Sensor Analog Out  │ ADC Input (3.3V max!)
GPIO 25   │ Relay 1 (Air Pump)    │ Digital OUT
GPIO 26   │ Relay 2 (Water Pump)  │ Digital OUT
GPIO 27   │ Relay 3 (LED Lights)  │ Digital OUT
GPIO 21   │ INA219 SDA (Power)    │ I²C SDA (shared)
GPIO 22   │ INA219 SCL (Power)    │ I²C SCL (shared)
3.3V      │ Sensor VCC            │ Power
GND       │ Common Ground         │ Ground
─────────────────────────────────────────────
```

**⚠️ Important:** pH sensor outputs 0–5V. Use a voltage divider (10kΩ + 20kΩ) to bring it to 0–3.3V before connecting to ESP32 ADC.

---

## Voltage Divider for pH Sensor

```
pH Sensor Out (5V max)
        │
       [10kΩ]
        │
        ├──────────► To ESP32 GPIO 34 (ADC)
        │
       [20kΩ]
        │
       GND

Output voltage = 5V × (20k / (10k + 20k)) = 3.33V max ✓
```

---

## Relay Wiring (Active LOW)

```
ESP32 GPIO 25 ──────► IN1 of Relay Module
                       │
                    ┌──▼────────────────┐
                    │  RELAY 1          │
                    │  NC ── (not used) │
                    │  COM ─── 12V+     │
                    │  NO  ─── Air Pump+│
                    └───────────────────┘
                    
(Air Pump - → 12V GND directly)

Repeat same pattern for Relay 2 (Water Pump) and Relay 3 (LED)
```

---

## Power Budget

| Component | Voltage | Current | Power |
|-----------|---------|---------|-------|
| Air pump | 12V | 0.5A | 6W |
| Water pump | 12V | 0.3A | 3.6W |
| ESP32 | 3.3V | 0.24A | 0.8W |
| LED grow lights (opt.) | 12V | 2.0A | 24W |
| Sensors total | 3.3V | 0.05A | 0.17W |
| **Total load** | — | — | **~35W max** |

With a 100W panel and 50Ah battery:
- Daytime: Panel covers all loads + charges battery
- Nighttime: Battery provides ~12–14 hours of sensor+pump operation

---

## Thermoelectric Generator (Optional Add-on)

```
Panel Back Surface (Hot ~50°C)
         │
    ┌────▼────┐
    │   TEG   │  ← TEC1-12706 or similar
    │ Module  │
    └────┬────┘
         │
    Ambient Air (Cold ~30°C)
    
TEG Output: ~0.5–2W (depends on ΔT)
Connect through boost converter (MT3608) to charge 5V USB or 12V bus
```

---

## Safety Notes

1. **Fuse:** Install a 10A automotive fuse between battery positive and load bus.
2. **Reverse polarity protection:** Add a P-channel MOSFET or Schottky diode on the battery line.
3. **Waterproofing:** All electronics must be in IP65-rated enclosures. Algae water and electronics don't mix.
4. **Ventilation:** The algae culture releases O₂. Ensure the installation area is well-ventilated.
5. **Ground fault:** Solar panels on rooftops should be properly grounded to building earth.
