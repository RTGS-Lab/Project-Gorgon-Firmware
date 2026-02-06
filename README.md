# GORGON - SDI-12 RTD Multiplexer

A 7-channel RTD temperature measurement system with SDI-12 sensor interface for the Raspberry Pi Pico.

## Overview

GORGON is an embedded firmware that implements a precision multichannel RTD (Resistance Temperature Detector) data acquisition system. It measures temperatures from up to 7 independent PT100 RTD sensors and exposes them via the industry-standard SDI-12 protocol, making it compatible with environmental data loggers and SCADA systems.

### Key Features

- **7-channel RTD measurement** with PT100 4-wire configuration
- **SDI-12 protocol compliance** (1200 baud) for sensor communication
- **24-bit precision ADC** (AD7124-8) with ratiometric measurement
- **Per-channel calibration** with persistent NVM storage
- **Production calibration mode** with Python automation tools
- **Internal temperature sensors** (Pico + ADC) for diagnostics

## Hardware

### Components

| Component | Part Number | Description |
|-----------|-------------|-------------|
| MCU | Raspberry Pi Pico (RP2040) | Dual-core ARM Cortex-M0+ @ 133MHz |
| ADC | AD7124-8 | 24-bit Sigma-Delta ADC, 8 differential channels |
| Mux | ADG708 | 8-channel CMOS analog multiplexer |
| RTD | PT100 | 100Ω @ 0°C, α = 0.00385 °C⁻¹ |
| R_ref | 5030Ω (nominal) | Precision reference resistor |

### Pin Configuration

| Function | GPIO | Notes |
|----------|------|-------|
| SPI MISO | 16 | ADC data out |
| SPI CS | 17 | ADC chip select (active low) |
| SPI SCK | 18 | 1 MHz clock |
| SPI MOSI | 19 | ADC data in |
| SDI-12 Data | 10 | Requires 3.3V↔5V level shifter |
| Mux Enable | 20 | Active high |
| Mux A0 | 21 | Channel select bit 0 |
| Mux A1 | 22 | Channel select bit 1 |
| Mux A2 | 23 | Channel select bit 2 |
| UART TX | 0 | Debug output (115200 baud) |
| UART RX | 1 | Debug input |

### Power Requirements

- **USB**: Programming and debug
- **12V external**: Sensor operation (current limit: 130mA)

## Building

### Prerequisites

- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk) (v2.2.0+)
- ARM GCC Toolchain
- CMake 3.13+

### Build Steps

```bash
# Clone and enter directory
cd sdi12-analog-mux

# Create build directory
mkdir build && cd build

# Configure
cmake ..

# Build
make -j4
```

Output: `build/sdi12-analog-mux.uf2`

### Flashing

1. Hold BOOTSEL button (or short BOOTSEL jumper) while connecting USB
2. Copy `sdi12-analog-mux.uf2` to the mounted RPI-RP2 drive
3. Device reboots automatically

## SDI-12 Interface

The device responds to standard SDI-12 commands at address `0`.

### Supported Commands

| Command | Response | Description |
|---------|----------|-------------|
| `0!` | `0` | Acknowledge |
| `0I!` | `013GEMSGORGO1.0 001` | Device identification |
| `0M1!` | `00011` | Start RTD channel 1 measurement |
| `0M2!` | `00011` | Start RTD channel 2 measurement |
| ... | ... | ... |
| `0M7!` | `00011` | Start RTD channel 7 measurement |
| `0M8!` | `00011` | Pico internal temperature |
| `0M9!` | `00011` | AD7124 internal temperature |
| `0D0!` | `0+25.123` | Retrieve measurement data |

### Identification String Format

```
013GEMSGORGO1.0 001
│││   │     │   │
││└───│─────│───│── SDI-12 version (1.3)
│└────│─────│───│── Vendor: GEMS
└─────│─────│───│── Address: 0
      └─────│───│── Model: GORGON (truncated)
            └───│── Version: 1.0
                └── Serial: 001
```

### Example Session

```
# Request identification
0I!
0013GEMSGORGO1.0 001

# Measure channel 1
0M1!
00011          # Response: ready in 001 seconds, 1 value

# Wait for measurement, then retrieve
0D0!
0+23.456       # Temperature in °C
```

## UART Commands

Connect via serial terminal at 115200 baud (8N1) for debug and configuration.

### NVM Commands

| Command | Description |
|---------|-------------|
| `nvm_dump` | Display all NVM data |
| `nvm_reset` | Factory reset |
| `nvm_set_sn <serial>` | Set device serial number |
| `nvm_set_date YYYY-MM-DD` | Set manufacturing date |
| `nvm_set_board <revision>` | Set board revision |
| `nvm_set_hw_ver <version>` | Set hardware version |
| `nvm_set_fw_ver <version>` | Set firmware version |
| `nvm_cal_rref <ch> <value>` | Set reference resistor (Ω) |
| `nvm_cal_offset <ch> <value>` | Set offset calibration |
| `nvm_cal_scale <ch> <value>` | Set scale factor |

### Calibration Mode Commands

| Command | Description |
|---------|-------------|
| `cal_mode_start` | Enter calibration mode (disables SDI-12) |
| `cal_mode_stop` | Exit calibration mode |
| `cal_set_serial <sn>` | Set serial number |
| `cal_set_hw_ver <ver>` | Set hardware version |
| `cal_set_fw_ver <ver>` | Set firmware version |
| `cal_set_date YYYY-MM-DD` | Set manufacturing date |
| `cal_measure <channel>` | Measure RTD channel (1-7) |

## Calibration

Production calibration requires precision reference resistors and the Python calibration tool.

### Quick Start

```bash
cd calibration
pip install pyserial
./calibrate.py /dev/ttyACM0 -s 001 -d 10 -i 10
```

See [calibration/README.md](calibration/README.md) for the complete calibration process.

## Project Structure

```
sdi12-analog-mux/
├── CMakeLists.txt           # Build configuration
├── pico_sdk_import.cmake    # Pico SDK integration
├── sdi12-analog-mux.c       # Main application
├── sdi12.c / sdi12.h        # SDI-12 protocol implementation
├── sdi12_uart.pio           # PIO assembly for 1200 baud UART
├── ad1724.c / ad1724.h      # AD7124-8 ADC driver
├── adg708.c / adg708.h      # ADG708 multiplexer driver
├── nvm.c / nvm.h            # Non-volatile memory storage
├── calibration_mode.c/.h    # Production calibration mode
└── calibration/
    ├── calibrate.py         # Calibration automation script
    ├── README.md            # Calibration quick reference
    └── CALIBRATION_GUIDE.md # Detailed calibration guide
```

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

### Measurement Optimization

- **Dummy read**: Charges input capacitors before actual measurement
- **Settling time**: 20ms delay after mux switching
- **Timeout protection**: 100 attempts × 10ms for ADC conversion

## License

[Add your license here]
