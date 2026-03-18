# GORGON Project Handoff Documentation

**Project**: GORGON - SDI-12 Underground Temperature Sensor
**Lab**: RTGS Lab
**Last Updated**: March 2026

---

## Quick Start for New Maintainers

This document provides everything you need to take over the GORGON project - a precision 7-channel RTD temperature measurement system using the SDI-12 protocol for underground temperature profiling.

### What This Project Does

GORGON is an environmental sensor that:
- Measures temperature from 7 independent PT100 RTD sensors (4-wire configuration)
- Communicates via industry-standard SDI-12 protocol (compatible with data loggers)
- Achieves 24-bit precision using the AD7124-8 ADC
- Runs on Raspberry Pi Pico (RP2040 microcontroller)
- Deploys in weatherproof enclosures for field use

### Applications
- Soil temperature profiling at multiple depths
- Permafrost and active layer monitoring
- Geothermal gradient measurement
- Agricultural soil monitoring
- Infrastructure temperature monitoring (roads, pipelines)

---

## Repository Structure

```
Project-Gorgon-Firmware/           # This repository
├── PROJECT_HANDOFF.md             # This file - start here
├── README.md                      # Firmware documentation
├── sdi12-analog-mux.c             # Main application
├── ad1724.c / ad1724.h            # AD7124-8 ADC driver
├── sdi12.c / sdi12.h              # SDI-12 protocol implementation
├── sdi12_uart.pio                 # PIO assembly for 1200 baud UART
├── adg708.c / adg708.h            # ADG708 multiplexer driver
├── nvm.c / nvm.h                  # Non-volatile memory storage
├── calibration_mode.c/.h          # Production calibration mode
├── CMakeLists.txt                 # Build configuration
├── calibration/
│   ├── calibrate.py               # Calibration automation script
│   ├── README.md                  # Calibration quick reference
│   └── CALIBRATION_GUIDE.md       # Detailed calibration guide
└── tools/
    ├── sdi12-logger/              # Temperature data logging tool
    ├── rtd-logger/                # RTD resistance logger
    └── serial-plotter/            # Real-time visualization
```

### Related Repositories

| Repository | URL | Contents |
|------------|-----|----------|
| Hardware (PCB) | https://github.com/RTGS-Lab/Project-Gorgon-Hardware | KiCad schematic, PCB, 3D enclosure, manufacturing files |
| Firmware (this repo) | https://github.com/zradlicz/Project-Gorgon-Firmware | Embedded code, calibration tools, Python tools |

---

## Archived Test Data

Historical test data (CSV files from November-December 2025) has been moved to Google Drive for archival:

**Location**: `radlicz-transition-folder` on Google Drive

**Archived directories** (originally in `tools/sdi12-logger/`):
- `1000uACurrentTest` - High current (1000µA) testing
- `100ohm_test_Mon_Nov24` - Reference resistor test
- `100ohm_test_Week_Dec02-09` - Week-long reference resistor test
- `100ohm_test_Week_Nov25-Dec02` - Week-long reference resistor test
- `250uACurrentKestrel` - Medium current testing with Kestrel logger
- `50uACurrent1N4148NoBreadboard` - Low current testing
- `50uACurrent1N4148NoBreadBoardKestrel` - Kestrel integration test
- `50uACurrent1N4148NoBreadboardShort` - Short duration test
- `50uACurrent1N4148TestShort` - Short test run
- `50uACurrentKestrel_Fri_Nov21` - Friday test session
- `50uACurrentKestrel_Sat_Nov22` - Saturday test session
- `50uACurrentKestrel_Sun_Nov23` - Sunday test session
- `50uACurrentTest` - General low current testing
- `Derivative_Analysis_Dec02-09` - Rate-of-change analysis
- `RTD_Analysis_Fri_Dec06` through `RTD_Analysis_Wed_Nov27` - Daily analysis folders

Each folder contains raw CSV data, analysis scripts, and generated plots.

---

## Getting Started

### 1. Build the Firmware

**Prerequisites:**
- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk) (v2.2.0+)
- ARM GCC toolchain
- CMake 3.13+

**Build:**
```bash
cd Project-Gorgon-Firmware
mkdir build && cd build
cmake ..
make -j4
```

**Output:** `build/sdi12-analog-mux.uf2`

### 2. Flash the Firmware

1. Short the BOOTSEL jumper (two pins next to micro USB) while connecting USB
2. Copy `sdi12-analog-mux.uf2` to the mounted `RPI-RP2` drive
3. Device reboots automatically

### 3. Verify Operation

Connect via serial terminal:
```bash
# Debug UART (115200 baud)
screen /dev/ttyACM0 115200

# Or for SDI-12 testing via USB
python3 tools/sdi12-logger/sdi12_logger.py --port /dev/ttyACM0
```

---

## Key Documentation Files

| Document | Location | Purpose |
|----------|----------|---------|
| Firmware README | `README.md` | Build, flash, SDI-12 commands, pin configuration |
| Calibration Guide | `calibration/CALIBRATION_GUIDE.md` | Complete production calibration procedure |
| Logger README | `tools/sdi12-logger/README.md` | Temperature data logging tool usage |
| RTD Logger README | `tools/rtd-logger/README.md` | Resistance logging for precision analysis |
| Serial Plotter README | `tools/serial-plotter/README.md` | Real-time visualization |
| Hardware README | (in Hardware repo) | PCB specs, components, manufacturing |

---

## Hardware Overview

### Key Components

| Component | Part Number | Function |
|-----------|-------------|----------|
| MCU | RP2040 (Raspberry Pi Pico) | Main processor, dual-core 133MHz |
| ADC | AD7124-8 | 24-bit sigma-delta ADC, 8 channels |
| Mux | ADG708 | 8-channel analog multiplexer |
| Flash | W25Q128JVS | 128Mb SPI flash for config storage |
| Level Shifter | TXS0102DCT | 3.3V ↔ 5V for SDI-12 |

### Pin Connections

| Function | GPIO | Notes |
|----------|------|-------|
| SPI MISO | 16 | ADC data out |
| SPI CS | 17 | ADC chip select (active low) |
| SPI SCK | 18 | 1 MHz clock |
| SPI MOSI | 19 | ADC data in |
| SDI-12 Data | 10 | Requires level shifter (3.3V↔5V) |
| Mux Enable | 20 | Active high |
| Mux A0-A2 | 21-23 | Channel select bits |
| UART TX/RX | 0/1 | Debug (115200 baud) |

### Known Hardware Issues (v1.0)

1. **Incorrect Diodes**: Replace with `1N4148WS-FDICT-ND` (75V 150mA, SOD-323)
2. **Missing Input Capacitor**: Add 1000µF 16V electrolytic between 12V input and GND

---

## SDI-12 Command Reference

The device uses address `0` by default.

| Command | Response | Description |
|---------|----------|-------------|
| `0!` | `0` | Acknowledge |
| `0I!` | `013GEMSGORGO1.0 001` | Device identification |
| `0M1!` | `00011` | Measure RTD channel 1 |
| `0M2!`-`0M7!` | `00011` | Measure RTD channels 2-7 |
| `0M8!` | `00011` | Pico internal temperature |
| `0M9!` | `00011` | ADC internal temperature |
| `0D0!` | `0+25.123` | Retrieve measurement data |

### Example SDI-12 Session

```
0I!                          # Request identification
0013GEMSGORGO1.0 001         # Response with vendor/model/version

0M1!                         # Start measurement on channel 1
00011                        # Ready in 001 seconds, 1 value

0D0!                         # Retrieve data
0+23.456                     # Temperature in °C
```

---

## Calibration Procedure

Full details: `calibration/CALIBRATION_GUIDE.md`

### Quick Calibration

```bash
cd calibration

# Quick test (1 minute)
./calibrate.py /dev/ttyACM0 -s TEST-001 -d 1 -i 10

# Production calibration (10 minutes)
./calibrate.py /dev/ttyACM0 -s DEVICE-12345 --hw-version 1.0 --fw-version 1.0
```

### Requirements
- Python 3.6+ with pyserial
- Precision reference resistors with known values (~100Ω, measured with calibrated multimeter)
- Room temperature environment (no ice bath required)

---

## Python Tools

### SDI-12 Logger (Temperature)

```bash
cd tools/sdi12-logger

# Install dependencies
pip install pyserial

# Log temperatures every 5 seconds
./sdi12_logger.py --port /dev/ttyACM0 --interval 5
```

Output: CSV with timestamps and all 9 temperature channels (M1-M9)

### RTD Logger (Resistance)

```bash
cd tools/rtd-logger

# Log RTD resistance values
python3 rtd_logger.py --port /dev/ttyACM0

# Analyze data
python3 analyze_data.py data/rtd_data_*.csv
```

### Serial Plotter (Real-time)

```bash
cd tools/serial-plotter

# Real-time visualization
python3 rtd_plotter.py --port /dev/ttyACM0
```

---

## Troubleshooting

### Serial Port Permission (Linux)

```bash
sudo usermod -a -G dialout $USER
# Log out and back in
```

### No Response from Device

1. Check USB connection
2. Verify correct serial port: `ls /dev/ttyACM* /dev/ttyUSB*`
3. Test with serial terminal: `screen /dev/ttyACM0 115200`
4. Ensure firmware is flashed correctly

### Measurement Returns FAIL

- Check RTD physical connections (4-wire: 2 current, 2 sense)
- Verify mux is switching (GPIO 20-23)
- Check ADC configuration in firmware
- Ensure power supply is adequate (5V, 130mA limit)

### High Standard Deviation in Calibration

- Environment not stable (allow system to reach thermal equilibrium)
- Poor electrical connections
- Electromagnetic interference
- Increase measurement duration and interval

### Build Errors

```bash
# Ensure Pico SDK is set up
export PICO_SDK_PATH=/path/to/pico-sdk

# Clean rebuild
cd build && rm -rf * && cmake .. && make -j4
```

---

## Technical Details

### Measurement Principle

The system uses ratiometric 4-wire RTD measurement:

1. 50µA excitation current flows through RTD and reference resistor
2. ADC measures voltage ratio: V_rtd / V_ref
3. RTD resistance calculated: R_rtd = R_ref × (V_rtd / V_ref)
4. Temperature derived using Callendar-Van Dusen equation

### NVM Data Structure

Calibration and manufacturing data are stored in flash with CRC32 protection:

- **Magic number**: 0x5344494D ("SDIM")
- **Version**: Structure version for forward compatibility
- **Manufacturing**: Serial, date, board revision, HW/FW versions
- **Calibration**: Per-channel R_ref, offset, scale; ADC offset/gain; temp coefficient

---

## Design Tools

- **PCB**: KiCad 8
- **Mechanical**: FreeCAD
- **Firmware**: VS Code + Pico SDK
- **Analysis**: Python with matplotlib, pandas, numpy

---

## Checklist for New Maintainer

- [ ] Clone this firmware repo
- [ ] Clone hardware repo from https://github.com/RTGS-Lab/Project-Gorgon-Hardware
- [ ] Set up Raspberry Pi Pico SDK
- [ ] Successfully build firmware
- [ ] Flash and test a device
- [ ] Run calibration procedure on a test unit
- [ ] Collect and analyze temperature data using Python tools
- [ ] Review all README files in each directory
- [ ] Access archived test data from Google Drive if needed
- [ ] Understand known hardware issues and fixes

---

*This handoff documentation was prepared for the RTGS Lab project transition.*
