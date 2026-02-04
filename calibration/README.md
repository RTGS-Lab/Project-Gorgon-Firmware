# GORGON RTD Calibration Tool

This directory contains the calibration tool for the GORGON SDI-12 Analog Multiplexer.

## Requirements

- VS Code with Pico extension installed
- Pico debugger (picoprobe or similar)
- 12V power supply with current limiting
- Python 3 with pyserial:
  ```bash
  pip install pyserial
  ```

## Complete Calibration Process

### Step 1: Flash Firmware

1. **Connect USB** to the GORGON board
2. **Connect 12V power supply** to 12V and GND pins (set current limit to 130mA)
3. **Short the BOOTSEL jumper** (two pins next to USB on the board)
4. **Turn on power** - RPI-RP2 drive should mount to the file system
5. **Build firmware** in VS Code using the Pico extension "Compile Project" command
6. **Copy the .uf2 file** from `build/sdi12-analog-mux.uf2` to the RPI-RP2 drive
7. **Wait for automatic unmount**
8. **Turn off power** and remove the BOOTSEL jumper

### Step 2: Start Debugger

1. **Plug in the debugger** to both the GORGON board and your computer
2. **Turn power back on**
3. **Press "Debug Project"** in VS Code - debugger should stop at `int main()`
4. **Press the play button** - firmware is now running

### Step 3: Measure Reference Resistors

1. **Measure each reference resistor** with a multimeter
2. **Update the values** in `calibrate.py` (around line 64):
   ```python
   REFERENCE_RESISTANCES = {
       1: 99.82,  # Replace with your measured values
       2: 99.80,
       3: 99.85,
       4: 99.88,
       5: 99.87,
       6: 99.81,
       7: 99.76,
   }
   ```

### Step 4: Run Calibration

1. Navigate to the calibration directory:
   ```bash
   cd /path/to/sdi12-analog-mux/calibration
   ```

2. Find the debugger serial port:
   ```bash
   ls /dev/ttyACM*
   ```

3. Run calibration (10 minutes, 10-second intervals):
   ```bash
   ./calibrate.py /dev/ttyACM0 -s 001 -d 10 -i 10
   ```

   Replace `/dev/ttyACM0` with your debugger's serial port and `001` with the device's 3-digit serial number.

4. **Wait for calibration to complete**

5. **Type `yes`** when prompted to write calibration to NVM

### Step 5: Finish

1. Turn off power
2. Unplug debugger and USB
3. Device is now calibrated

## Command Reference

```bash
./calibrate.py <port> -s <serial_number> [options]

Required:
  port                  Debugger serial port (e.g., /dev/ttyACM0)
  -s, --serial          Device serial number (3 digits, e.g., 001)

Options:
  -d, --duration        Calibration duration in minutes (default: 10)
  -i, --interval        Measurement interval in seconds (default: 30)
  --hw-version          Hardware version (default: 1.0)
  --fw-version          Firmware version (default: 1.0)
  --board               Board name (default: GORGON)
  --no-write            Dry run - skip writing to device
  --read-nvm            Read and display current NVM contents only
```

## Examples

Standard calibration:
```bash
./calibrate.py /dev/ttyACM0 -s 003 -d 10 -i 10
```

Quick test (1 minute):
```bash
./calibrate.py /dev/ttyACM0 -s 003 -d 1 -i 10 --no-write
```

Read current NVM values:
```bash
./calibrate.py /dev/ttyACM0 --read-nvm
```

## Reference Resistances

The calibration uses precision reference resistors. Current values configured in `calibrate.py`:

| Channel | Reference Resistance |
|---------|---------------------|
| 1       | 99.82 Ω             |
| 2       | 99.80 Ω             |
| 3       | 99.85 Ω             |
| 4       | 99.88 Ω             |
| 5       | 99.87 Ω             |
| 6       | 99.81 Ω             |
| 7       | 99.76 Ω             |

To update these values, edit the `REFERENCE_RESISTANCES` dict in `calibrate.py` (around line 64).

## Troubleshooting

**No serial port found:**
- Ensure debugger is connected and powered
- Check port with `ls /dev/ttyACM*`
- Try `sudo chmod 666 /dev/ttyACM0` if permission denied

**Device not entering BOOTSEL mode:**
- Ensure BOOTSEL jumper is shorted before powering on
- Check USB connection

**Calibration measurements failing:**
- Check RTD reference resistor connections
- Verify firmware is running (use debugger play button)
- Check current limit is set to 130mA

**High standard deviation in results:**
- Increase calibration duration (-d 15 or higher)
- Check for loose connections
