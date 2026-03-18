# SDI-12 Temperature Logger

Continuously logs temperature data from SDI-12 sensors via an Adafruit Feather USB-serial bridge. Supports measurements from M1-M9 commands including RTD sensors and internal temperature sensors.

## Features

- Logs data from 9 temperature sources:
  - M1: RTD1 temperature
  - M2: RTD2 temperature
  - M3: RTD3 temperature
  - M4: RTD4 temperature
  - M5: RTD5 temperature
  - M6: RTD6 temperature
  - M7: RTD7 temperature
  - M8: Raspberry Pi Pico internal temperature
  - M9: AD7124 ADC internal temperature
- Continuous logging with configurable interval
- Timestamped CSV output
- Real-time console feedback

## Requirements

- Python 3.6+
- Adafruit Feather (or similar) as USB-serial bridge
- SDI-12 sensor connected to Feather

## Installation

1. Install dependencies:
```bash
pip install -r requirements.txt
```

Or manually:
```bash
pip install pyserial
```

## Usage

### Basic Usage

```bash
./sdi12_logger.py --port /dev/ttyACM0
```

This will:
- Connect to the Feather on `/dev/ttyACM0`
- Log all measurements (M1-M9) every 5 seconds
- Save to auto-generated CSV file with timestamp

### Custom Interval

Log every 10 seconds:
```bash
./sdi12_logger.py --port /dev/ttyACM0 --interval 10
```

### Custom Output File

Specify output filename:
```bash
./sdi12_logger.py --port /dev/ttyACM0 --output my_temps.csv
```

### Different SDI-12 Address

If your sensor uses a different address (e.g., '1'):
```bash
./sdi12_logger.py --port /dev/ttyACM0 --address 1
```

### All Options

```
usage: sdi12_logger.py [-h] --port PORT [--address ADDRESS] [--baud BAUD]
                       [--interval INTERVAL] [--output OUTPUT]

optional arguments:
  -h, --help            show this help message and exit
  --port PORT, -p PORT  Serial port for Adafruit Feather (e.g., /dev/ttyACM0)
  --address ADDRESS, -a ADDRESS
                        SDI-12 sensor address (default: 0)
  --baud BAUD, -b BAUD  SDI-12 baud rate (default: 1200, not used for USB)
  --interval INTERVAL, -i INTERVAL
                        Logging interval in seconds (default: 5.0)
  --output OUTPUT, -o OUTPUT
                        Output CSV file (default: auto-generated with timestamp)
```

## Output Format

The logger creates a CSV file with the following columns:

| Column | Description |
|--------|-------------|
| `timestamp` | Unix timestamp (seconds since epoch) |
| `datetime` | Human-readable date and time |
| `M1_RTD1_temp_C` | RTD1 temperature in °C |
| `M2_RTD2_temp_C` | RTD2 temperature in °C |
| `M3_RTD3_temp_C` | RTD3 temperature in °C |
| `M4_RTD4_temp_C` | RTD4 temperature in °C |
| `M5_RTD5_temp_C` | RTD5 temperature in °C |
| `M6_RTD6_temp_C` | RTD6 temperature in °C |
| `M7_RTD7_temp_C` | RTD7 temperature in °C |
| `M8_Pico_temp_C` | Raspberry Pi Pico internal temperature in °C |
| `M9_ADC_temp_C` | AD7124 ADC internal temperature in °C |

### Example Output

```csv
timestamp,datetime,M1_RTD1_temp_C,M2_RTD2_temp_C,M3_RTD3_temp_C,M4_RTD4_temp_C,M5_RTD5_temp_C,M6_RTD6_temp_C,M7_RTD7_temp_C,M8_Pico_temp_C,M9_ADC_temp_C
1699635421.5,2023-11-10 14:30:21,23.45,24.12,23.89,22.78,23.56,24.01,23.34,25.67,26.34
1699635426.5,2023-11-10 14:30:26,23.46,24.13,23.90,22.79,23.57,24.02,23.35,25.68,26.35
1699635431.5,2023-11-10 14:30:31,23.47,24.14,23.91,22.80,23.58,24.03,23.36,25.69,26.36
```

## How It Works

The logger follows the SDI-12 protocol for each measurement:

1. Send measurement command (e.g., `0M1!`)
2. Wait for sensor to indicate readiness
3. Send data request (`0D0!`)
4. Parse temperature value from response
5. Repeat for all sensors (M1-M9)
6. Write all values to CSV file
7. Wait for interval period
8. Repeat

## Finding Your Serial Port

### Linux
```bash
# List all serial ports
ls /dev/ttyACM* /dev/ttyUSB*

# Or use dmesg to see what was just connected
dmesg | tail
```

### macOS
```bash
ls /dev/tty.usb*
```

### Windows
Check Device Manager or use:
```cmd
mode
```

## Troubleshooting

### No Response from Sensor

1. Check serial port connection
2. Verify correct SDI-12 address (use `--address` option)
3. Ensure Feather firmware is properly communicating with SDI-12 sensor
4. Check physical connections and power

### Permission Denied (Linux)

Add yourself to the dialout group:
```bash
sudo usermod -a -G dialout $USER
# Log out and back in for changes to take effect
```

Or run with sudo (not recommended):
```bash
sudo ./sdi12_logger.py --port /dev/ttyACM0
```

### Partial Data

If some measurements succeed but others fail:
- Check sensor configuration (ensure M1-M9 are properly implemented in firmware)
- Some RTD channels (M4-M7) may not be physically connected
- Increase timeout values in the code if needed
- Check for electrical noise or connection issues

## Stopping the Logger

Press `Ctrl+C` to stop logging. The data will be saved to the CSV file.

## Tips

- Use `tail -f sdi12_data_*.csv` to monitor the log file in real-time
- Import CSV into Excel, LibreOffice, or Python pandas for analysis
- For long-term logging, consider using `screen` or `tmux` to keep session alive
- Add timestamps to filename for multiple logging sessions

## Example Analysis (Python)

```python
import pandas as pd
import matplotlib.pyplot as plt

# Load data
df = pd.read_csv('sdi12_data_20231110_143021.csv')
df['datetime'] = pd.to_datetime(df['datetime'])

# Plot all temperatures
df.plot(x='datetime',
        y=['M1_RTD1_temp_C', 'M2_RTD2_temp_C', 'M3_RTD3_temp_C',
           'M4_RTD4_temp_C', 'M5_RTD5_temp_C', 'M6_RTD6_temp_C',
           'M7_RTD7_temp_C', 'M8_Pico_temp_C', 'M9_ADC_temp_C'],
        figsize=(12, 6))
plt.ylabel('Temperature (°C)')
plt.title('SDI-12 Temperature Measurements')
plt.grid(True)
plt.show()
```

## License

This project is part of the pico-debug repository.
