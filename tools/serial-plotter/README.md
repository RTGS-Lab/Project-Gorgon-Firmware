# RTD Serial Plotter

Real-time plotting tool for RTD resistance and temperature measurements from the Pico RTD measurement system.

## Features

- Real-time plotting of resistance values
- Temperature calculation and plotting
- Dark theme for better visibility
- Configurable serial port and baud rate
- Auto-scaling axes
- Live value display in title

## Installation

1. Install Python dependencies:
```bash
pip install -r requirements.txt
```

## Usage

### Basic usage (default settings):
```bash
python rtd_plotter.py
```

### Specify serial port:
```bash
python rtd_plotter.py --port /dev/ttyUSB0
```

### Custom baud rate:
```bash
python rtd_plotter.py --port /dev/ttyACM0 --baud 115200
```

### Limit displayed points:
```bash
python rtd_plotter.py --points 500
```

## Default Settings

- **Port**: `/dev/ttyACM0` (typical for Pico)
- **Baud Rate**: `115200`
- **Max Points**: `1000`

## Serial Data Format

The plotter looks for lines containing:
```
RTD resistance calculated: XXX.XX ohms
```

## Controls

- **Close window** or **Ctrl+C** to stop
- Plots auto-scale to show all data
- Real-time values shown in window title

## Troubleshooting

### Permission denied on serial port:
```bash
sudo usermod -a -G dialout $USER
# Then log out and back in
```

### Port not found:
Check available ports:
```bash
ls /dev/tty*
```

### No data appearing:
- Verify Pico is connected and running
- Check baud rate matches Pico output
- Ensure printf statements include "RTD resistance calculated"