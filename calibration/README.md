# RTD Calibration Tool

This directory contains the calibration tool for the SDI-12 Analog Multiplexer.

## Overview

The calibration process:
1. Connects to the device via UART
2. Enters calibration mode on the device
3. Collects resistance measurements from all 7 RTD channels over 10 minutes
4. Calculates calibrated R_ref values for each channel
5. Writes the calibration data to the device's non-volatile memory
6. Generates a calibration report

## Requirements

```bash
pip install pyserial
```

## Usage

### Calibration Mode

Basic usage:
```bash
./calibrate.py /dev/ttyUSB0 -s DEVICE001
```

Full options:
```bash
./calibrate.py <port> -s <serial_number> [options]

Arguments:
  port                  Serial port (e.g., /dev/ttyUSB0 or COM3)
  -s, --serial         Device serial number (required for calibration)

Options:
  -b, --baudrate       Baud rate (default: 115200)
  -d, --duration       Calibration duration in minutes (default: 10)
  -i, --interval       Measurement interval in seconds (default: 30)
  --hw-version         Hardware version (default: 1.0)
  --fw-version         Firmware version (default: 1.0)
  --no-write           Skip writing to device (dry run)
  --read-nvm           Read and display NVM contents (no calibration)
```

### Read NVM Mode

To read current NVM contents without calibrating:
```bash
./calibrate.py /dev/ttyUSB0 --read-nvm
```

This displays:
- Device serial number
- Hardware and firmware versions
- Manufacturing date
- All calibration values (R_ref for each channel)
- NVM header information (magic, version, CRC32)

## Example

Calibrate device with serial number "TEST-001234":
```bash
./calibrate.py /dev/ttyUSB0 \
    -s TEST-001234 \
    --hw-version 1.0 \
    --fw-version 1.0 \
    -d 10 \
    -i 30
```

Short calibration for testing (1 minute):
```bash
./calibrate.py /dev/ttyUSB0 -s TEST-001234 -d 1 -i 10
```

Read current NVM values:
```bash
./calibrate.py /dev/ttyUSB0 --read-nvm
```

## Reference Resistances

The calibration uses the following known reference resistances (at 0°C):

| Channel | Reference Resistance (Ω) |
|---------|--------------------------|
| 1       | 99.88                    |
| 2       | 99.75                    |
| 3       | 99.80                    |
| 4       | 99.67                    |
| 5       | 99.84                    |
| 6       | 99.66                    |
| 7       | 99.78                    |

## Calibration Process

1. **Setup**: Connect all 7 RTD reference resistors to the device channels
2. **Start**: Run the calibration script with device serial number
3. **Data Collection**: Script collects measurements every 30 seconds for 10 minutes
4. **Calculation**: Calculates individual R_ref for each channel, then averages them
5. **Review**: Review calibration results and averaged R_ref value
6. **Write**: Averaged R_ref is written to all 7 channels in device NVM
7. **Report**: Calibration report is saved to file

**Note**: The calibration uses a single averaged R_ref value for all channels since they share the same reference resistor.

## Output Files

Calibration reports are saved as:
```
calibration_report_YYYYMMDD_HHMMSS.txt
```

The report includes:
- Calibration results summary
- Statistical analysis (mean, standard deviation)
- Calibrated R_ref values
- Raw measurement data

## Troubleshooting

**Connection Issues**:
- Check serial port permissions: `sudo chmod 666 /dev/ttyUSB0`
- Verify correct port: `ls /dev/ttyUSB*`
- Check baud rate matches firmware (115200)

**Measurement Failures**:
- Ensure device is not in SDI-12 mode during calibration
- Check RTD connections
- Verify reference resistors are at stable temperature (0°C ice bath)

**Calibration Mode Not Entering**:
- Update firmware with calibration mode support
- Check UART output for error messages
- Verify firmware version supports calibration commands

## Firmware Commands

The calibration script uses these firmware commands:

- `cal_mode_start` - Enter calibration mode
- `cal_mode_stop` - Exit calibration mode
- `cal_set_serial <sn>` - Set device serial number
- `cal_set_hw_ver <ver>` - Set hardware version
- `cal_set_fw_ver <ver>` - Set firmware version
- `cal_set_date <YYYY-MM-DD>` - Set manufacturing date
- `cal_measure <channel>` - Measure specific RTD channel (1-7)
