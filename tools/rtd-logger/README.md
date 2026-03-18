# RTD Data Logger

A Python tool for logging RTD (PT100) resistance measurements from the sdi12-analog-mux device to CSV files for precision and accuracy analysis.

## Overview

This tool captures resistance values from three RTDs (RTD1, RTD2, RTD3) via serial port and logs them to timestamped CSV files. It's designed for long-running data collection to enable statistical analysis of measurement precision and accuracy.

## Features

- Real-time serial data capture from sdi12-analog-mux device
- Automatic CSV logging with timestamps
- Live status updates during logging
- Comprehensive statistical analysis tool
- Precision metrics (standard deviation, coefficient of variation)
- Easy-to-use command-line interface

## Installation

### Prerequisites

- Python 3.6 or higher
- pip package manager

### Setup

1. Navigate to the rtd-logger directory:
```bash
cd rtd-logger
```

2. Install dependencies:
```bash
pip install -r requirements.txt
```

Or install in a virtual environment (recommended):
```bash
python3 -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate
pip install -r requirements.txt
```

## Usage

### 1. Data Collection

Start logging RTD data:

```bash
python3 rtd_logger.py --port /dev/ttyACM0
```

**Options:**
- `--port`, `-p`: Serial port (default: `/dev/ttyACM0`)
- `--baud`, `-b`: Baud rate (default: `115200`)
- `--output-dir`, `-o`: Output directory for CSV files (default: `data`)

**Example:**
```bash
python3 rtd_logger.py --port /dev/ttyUSB0 --baud 115200 --output-dir measurements
```

The logger will:
- Connect to the serial port
- Create a timestamped CSV file (e.g., `rtd_data_20231215_143022.csv`)
- Log RTD resistance values continuously
- Print status updates every 10 seconds
- Stop when you press `Ctrl+C`

### 2. Data Analysis

Analyze collected data:

```bash
python3 analyze_data.py data/rtd_data_20231215_143022.csv
```

The analysis tool provides:
- **Basic Statistics**: mean, median, min, max, range
- **Precision Metrics**: standard deviation, coefficient of variation (CV%)
- **Precision Assessment**: Automated evaluation of measurement quality
- **Accuracy Guidelines**: Notes on comparing to reference values

**Output Example:**
```
================================================================================
RTD MEASUREMENT STATISTICS
================================================================================

File: data/rtd_data_20231215_143022.csv
Samples: 1850
Duration: 3700.2 seconds (61.7 minutes)
Sample rate: 0.500 samples/second

--------------------------------------------------------------------------------
Metric               RTD1               RTD2               RTD3
--------------------------------------------------------------------------------
Mean (Ω)              100.0234          100.0189          100.0156
Median (Ω)            100.0230          100.0185          100.0152
Std Dev (Ω)             0.0425            0.0398            0.0412
Min (Ω)                99.9102           99.9078           99.9045
Max (Ω)               100.1456          100.1398          100.1367
Range (Ω)               0.2354            0.2320            0.2322
CV (%)                  0.0425            0.0398            0.0412
--------------------------------------------------------------------------------

================================================================================
PRECISION ANALYSIS
================================================================================

Coefficient of Variation (CV) indicates measurement precision:
  < 1%:  Excellent precision
  1-2%:  Good precision
  2-5%:  Moderate precision
  > 5%:  Poor precision

RTD1: CV = 0.0425% (Excellent)
RTD2: CV = 0.0398% (Excellent)
RTD3: CV = 0.0412% (Excellent)
```

## CSV File Format

The logger creates CSV files with the following columns:

| Column | Description |
|--------|-------------|
| `timestamp` | ISO 8601 timestamp of measurement |
| `elapsed_time_s` | Seconds since logging started |
| `rtd1_ohms` | RTD1 resistance in ohms |
| `rtd2_ohms` | RTD2 resistance in ohms |
| `rtd3_ohms` | RTD3 resistance in ohms |

**Example:**
```csv
timestamp,elapsed_time_s,rtd1_ohms,rtd2_ohms,rtd3_ohms
2023-12-15T14:30:22.123456,0.000,100.02,100.01,100.00
2023-12-15T14:30:24.234567,2.111,100.03,100.02,100.01
2023-12-15T14:30:26.345678,4.222,100.02,100.01,100.00
```

## Serial Data Format

The tool parses summary lines from the sdi12-analog-mux device:

```
Summary: RTD1=100.02Ω, RTD2=100.01Ω, RTD3=100.00Ω
```

## Tips for Long-Running Tests

1. **Stable Environment**: Keep RTDs in a thermally stable environment
2. **Run Time**: For meaningful statistics, collect at least 100-500 samples
3. **Monitoring**: Check status messages to ensure continuous data capture
4. **Storage**: Ensure adequate disk space for long tests
5. **Connection**: Use a reliable USB cable to prevent disconnections

## Understanding Precision vs Accuracy

- **Precision**: How repeatable measurements are (low standard deviation = high precision)
  - Measured by: Standard deviation, coefficient of variation
  - Example: Getting 100.02Ω, 100.03Ω, 100.02Ω repeatedly (precise)

- **Accuracy**: How close measurements are to the true value
  - Measured by: Comparing mean to known reference
  - Example: Mean of 100.02Ω when true value is 100.00Ω (error = 0.02%)

## Troubleshooting

**Serial Port Issues:**
- Linux: Ensure user is in `dialout` group: `sudo usermod -a -G dialout $USER`
- Check available ports: `ls /dev/tty*`
- Verify device connection: `dmesg | grep tty`

**No Data Being Logged:**
- Verify the sdi12-analog-mux device is running
- Check that serial port and baud rate are correct
- Ensure device is sending summary lines (check with `screen` or minicom)

**Permission Denied:**
```bash
sudo chmod 666 /dev/ttyACM0
```

## Files

- `rtd_logger.py` - Main data logging script
- `analyze_data.py` - Statistical analysis tool
- `requirements.txt` - Python dependencies
- `data/` - Default directory for CSV files (created automatically)

## License

This tool is part of the pico-debug project.
