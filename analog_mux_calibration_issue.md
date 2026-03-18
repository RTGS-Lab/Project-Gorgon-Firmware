# SDI12 Analog Mux Temperature Calibration Issue

## Summary

Three SDI12 Analog Mux devices connected to the Guadalupe station are reporting significantly different temperature values despite being physically located next to each other. The offsets are consistent (~2°C between each device) across all RTD channels, suggesting a systematic calibration issue.

## Device Information

- **Station**: Guadalupe
- **Node ID**: `e00fce6885951c63c0e86719`
- **Configuration**: 3 SDI12 Analog Mux units on SDI12 Talon port 1
- **Device Positions**: `[1, 1]`, `[1, 2]`, `[1, 3]`
- **Data Collection Period**: Feb 19, 2026, 02:00 - 16:00 UTC

## Observed Temperature Offsets

All three muxes should read the same temperature since they are co-located. Instead:

| RTD Sensor | Mux 1 [1,1] | Mux 2 [1,2] | Mux 3 [1,3] | Offset (Mux1→Mux3) |
|------------|-------------|-------------|-------------|---------------------|
| RTD1_Temp  | 20.6 - 22.5°C | 22.6 - 24.6°C | 23.5 - 25.2°C | +2.9°C |
| RTD2_Temp  | 20.4 - 21.8°C | 22.4 - 23.4°C | 23.1 - 24.4°C | +2.7°C |
| RTD3_Temp  | 20.0 - 21.4°C | 22.2 - 23.3°C | 23.2 - 24.0°C | +3.2°C |
| RTD4_Temp  | 19.9 - 21.5°C | 22.0 - 23.3°C | 22.9 - 23.9°C | +3.0°C |
| RTD5_Temp  | 19.8 - 21.1°C | 21.8 - 22.9°C | 22.5 - 23.8°C | +2.8°C |
| RTD6_Temp  | 19.7 - 21.1°C | 21.8 - 22.9°C | 22.6 - 23.8°C | +2.9°C |
| RTD7_Temp  | 19.6 - 21.1°C | 21.7 - 22.8°C | 22.5 - 23.7°C | +2.9°C |
| Pico_Temp  | 24.8 - 30.9°C | 22.5 - 27.1°C | 20.6 - 26.7°C | -4.2°C (inverted) |

**Key observations:**
- Mux 1 consistently reads ~2-3°C **colder** than Mux 2
- Mux 3 consistently reads ~2-3°C **warmer** than Mux 2
- The offset is consistent across all 7 RTD channels
- Pico_Temp (onboard microcontroller temp) shows the **opposite** pattern - Mux 1 is hottest

## Visualization

![Temperature comparison plot](../figures/guadalupe_analog_mux_by_device.png)

## Data Files

- Raw data: `data/Gems_Demo_2026_02_18_to_2026_02_19_23_59_59_20260219_101900.csv`
- Parsed data: `data/parsed/Gems_Demo_2026_02_18_to_2026_02_19_23_59_59_20260219_101900_parsed_20260219_102002.csv`
- Plot script: `scripts/plot_guadalupe_analog_mux.py`

## Firmware Investigation Areas

### 1. RTD Measurement Circuit
- What is the reference resistor value used for RTD measurements?
- Are reference resistors calibrated per-device or assumed identical?
- Is there a calibration offset stored in device EEPROM/flash?

### 2. ADC Configuration
- What ADC is used for RTD measurements?
- Is there per-device ADC calibration?
- What is the ADC reference voltage source? Could it vary between units?

### 3. Temperature Calculation
- What is the RTD-to-temperature conversion formula?
- Are there any hardcoded calibration constants?
- Is the Callendar-Van Dusen equation used? What coefficients?

### 4. Pico Temperature Anomaly
- The Pico (onboard) temp shows an inverted offset pattern compared to RTDs
- Could higher Pico temp on Mux 1 indicate higher current draw affecting RTD readings?
- Is there any self-heating compensation?

### 5. Hardware Variation
- Are there known component tolerances that could cause this?
- Is there a factory calibration procedure for these devices?
- Can calibration offsets be stored and applied in firmware?

## Questions to Answer

1. Where in the firmware is the RTD temperature calculated?
2. Is there a calibration data structure that could store per-device offsets?
3. What SDI12 commands are used to read temperature? (Check SDI12 address handling)
4. Could the SDI12 address assignment (`[1,1]`, `[1,2]`, `[1,3]`) affect readings?

## Reproduction

To regenerate the analysis:

```bash
cd /home/zach/Code/rtgs-lab-tools
python scripts/plot_guadalupe_analog_mux.py
```

## Device Configuration at Time of Test

```
System Configuration UID: 59001684
- Log Period: 900s
- Backhaul Count: 4
- Power Save Mode: 2
- Logging Mode: 3
- Num SDI12 Talons: 1

Sensor Configuration UID: 2101507
- Num Analog Mux: 3
- Num Soil Sensors: 2
- Num CO2 Sensors: 1
- Num O2 Sensors: 1
```
