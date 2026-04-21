# Troubleshooting Guide

## Electrical / Solar System

| Problem | Possible Cause | Fix |
|---------|---------------|-----|
| Battery not charging | Wrong MPPT polarity | Check PV+/PV- connections |
| MPPT shows no input | Panel voltage too low | Check wiring, measure Voc with multimeter |
| Battery draining fast | Load too high / night use | Reduce pump run time; add bigger battery |
| ESP32 rebooting randomly | Insufficient 3.3V from relay switching | Add 100µF capacitor on 3.3V rail; use separate 5V USB supply for ESP32 |
| pH sensor reading erratic | ADC noise | Increase averaging (50+ samples); add 0.1µF cap between ADC pin and GND |
| CO₂ sensor reads 5000+ | Not calibrated | Run outdoor calibration; check UART connections |

## Algae Culture

| Problem | Symptom | Fix |
|---------|---------|-----|
| Culture crash | Sudden turn to clear/white | Restart. Clean PBR with 10% bleach, re-inoculate |
| Slow growth | Stays pale green after 1 week | Check light levels (add LEDs), check nutrients |
| Contamination | Fuzzy growths, bad smell | Restart with sterile conditions; filter air inlet |
| Overheating | Temperature >38°C | Shade the PBR or add cooling water jacket |
| Biofilm on walls | Thick green crust | Normal; scrub during weekly maintenance |
| Culture settles | No circulation at night | Add timer to run water pump 15 min/hr at night |

## WiFi / MQTT

| Problem | Fix |
|---------|-----|
| ESP32 won't connect to WiFi | Check SSID/password in code; ensure 2.4GHz (not 5GHz only) |
| MQTT not publishing | Verify broker IP; test with MQTT Explorer tool |
| Dashboard shows no data | Check topic names match in code and dashboard config |

## General Tips

- **Always test sensors in water before sealing** the PBR permanently
- **Label all wires** — you'll thank yourself during maintenance
- **Keep a log** of pH, temp, OD₆₈₀ readings in a notebook or spreadsheet
- **Take photos** every 2 weeks to visually track algae density changes
