# SDI-12 Analog Multiplexer Calibration Guide

## Overview

This guide describes the complete calibration process for the SDI-12 Analog Multiplexer RTD measurement system.

## Prerequisites

### Hardware Setup
1. **Reference Resistors**: 7 precision reference resistors with known values (measured with calibrated multimeter)
   - Channel 1: 99.88 Ω
   - Channel 2: 99.75 Ω
   - Channel 3: 99.80 Ω
   - Channel 4: 99.67 Ω
   - Channel 5: 99.84 Ω
   - Channel 6: 99.66 Ω
   - Channel 7: 99.78 Ω

   **Note**: These are example values. Measure your actual reference resistors with a calibrated multimeter and update the script accordingly.

2. **Test Setup**:
   - Device powered via USB or external 5V supply
   - UART connection (115200 baud, 8N1)
   - All 7 RTD channels connected to reference resistors
   - Room temperature environment (no ice bath required)

### Software Requirements
```bash
# Python 3.6 or later
pip install pyserial
```

## Calibration Process

### Step 1: Hardware Preparation

1. **Reference Resistor Setup**:
   ```
   - Measure each reference resistor with a calibrated multimeter
   - Record the exact resistance values for each channel
   - Update the calibration script with your measured values
   - Connect resistors using 4-wire configuration
   ```

2. **Connect Device**:
   ```
   - Connect USB cable to device
   - Verify UART port appears (e.g., /dev/ttyUSB0)
   - Test connection: screen /dev/ttyUSB0 115200
   ```

3. **Verify RTD Connections**:
   ```
   - Connect all 7 reference resistors to device channels
   - Ensure proper 4-wire connections
   - Check for loose connections or shorts
   ```

### Step 2: Run Calibration Script

**Basic Calibration** (10 minutes, 30-second intervals):
```bash
cd /home/zach/Code/pico-debug/sdi12-analog-mux/calibration
./calibrate.py /dev/ttyUSB0 -s DEVICE-12345 --hw-version 1.0 --fw-version 1.0
```

**Quick Test** (1 minute, 10-second intervals):
```bash
./calibrate.py /dev/ttyUSB0 -s TEST-001 -d 1 -i 10
```

**Extended Calibration** (30 minutes, 60-second intervals):
```bash
./calibrate.py /dev/ttyUSB0 -s DEVICE-12345 -d 30 -i 60
```

**Dry Run** (no write to device):
```bash
./calibrate.py /dev/ttyUSB0 -s TEST-001 --no-write
```

### Step 3: Monitor Calibration Progress

The script will display real-time measurements:

```
=== Starting Data Collection ===
Duration: 10 minutes
Measurement interval: 30 seconds
Channels: [1, 2, 3, 4, 5, 6, 7]

Time            Ch1     Ch2     Ch3     Ch4     Ch5     Ch6     Ch7
--------------------------------------------------------------------------------
14:30:00        99.89   99.76   99.81   99.68   99.85   99.67   99.79
14:30:30        99.88   99.75   99.80   99.67   99.84   99.66   99.78
14:31:00        99.87   99.74   99.79   99.66   99.83   99.65   99.77
...
```

### Step 4: Review Calibration Results

After data collection completes:

```
=== Calibration Results ===

Channel | Ref Ω  | Measured Ω | Std Dev | Cal R_ref | Samples
----------------------------------------------------------------------
  1     | 99.88  | 99.8765    | 0.0234  | 5032.15   | 20
  2     | 99.75  | 99.7412    | 0.0189  | 5031.89   | 20
  3     | 99.80  | 99.7923    | 0.0211  | 5030.98   | 20
  4     | 99.67  | 99.6634    | 0.0245  | 5031.24   | 20
  5     | 99.84  | 99.8298    | 0.0198  | 5032.42   | 20
  6     | 99.66  | 99.6542    | 0.0223  | 5031.08   | 20
  7     | 99.78  | 99.7734    | 0.0206  | 5031.56   | 20
----------------------------------------------------------------------

Average Calibrated R_ref: 5031.62 Ω
Standard Deviation:       0.48 Ω
Channels Used:            7
```

**Quality Checks**:
- **Per-Channel Std Dev**: Should be < 0.05 Ω (good measurement stability)
- **Sample Count**: Should match expected (e.g., 20 samples for 10 min @ 30s)
- **Cal R_ref Range**: Typically 5025-5035 Ω (nominal 5030 Ω ± 0.1%)
- **Average R_ref Std Dev**: Should be < 1.0 Ω (channel-to-channel consistency)

**Note**: The script calculates an individual R_ref for each channel, then averages them into a single value. This averaged value is written to all 7 channels since they share the same physical reference resistor.

### Step 5: Write Calibration to Device

The script will prompt:
```
Write calibration to device NVM? (yes/no):
```

Type `yes` to write calibration values to non-volatile memory.

Output:
```
=== Writing Calibration to Device ===
Using averaged R_ref: 5031.62 Ω
Writing to all channels...

Channel 0: nvm_cal_rref 0 5031.62
  -> R_ref[0] set to: 5031.62 ohms
Channel 1: nvm_cal_rref 1 5031.62
  -> R_ref[1] set to: 5031.62 ohms
Channel 2: nvm_cal_rref 2 5031.62
  -> R_ref[2] set to: 5031.62 ohms
...

Calibration complete! R_ref = 5031.62 Ω written to all channels.
```

### Step 6: Verify Calibration

After writing, verify the calibration was saved. You have two options:

**Option 1: Using calibration script**
```bash
./calibrate.py /dev/ttyUSB0 --read-nvm
```

**Option 2: Using serial terminal**
```bash
# Connect to device via serial terminal
screen /dev/ttyUSB0 115200

# Type command:
nvm_dump
```

Expected output:
```
========================================
NVM Data Dump
========================================

Header:
  Magic:    0x5344494D (valid)
  Version:  1
  CRC32:    0x12345678

Manufacturing:
  Date:     2025-11-24
  Serial:   DEVICE-12345
  Board:    UNKNOWN
  HW Ver:   1.0
  FW Ver:   1.0

Calibration:
  R_ref values (ohms):
    Ch0: 5031.62
    Ch1: 5031.62
    Ch2: 5031.62
    Ch3: 5031.62
    Ch4: 5031.62
    Ch5: 5031.62
    Ch6: 5031.62
```

### Step 7: Test Measurements

Test that measurements are accurate:

```bash
# In serial terminal, measure each channel
cal_mode_start
cal_measure 1
cal_measure 2
...
cal_mode_stop
```

Compare measured values to reference resistances. Should match within ±0.05 Ω.

## Calibration Report

A calibration report is automatically saved:
```
calibration/calibration_report_YYYYMMDD_HHMMSS.txt
```

This report includes:
- Complete calibration results
- Statistical analysis
- Raw measurement data
- Calibration metadata (date, port, settings)

## Reading NVM Without Calibration

To check the current calibration values on a device without performing a new calibration:

```bash
cd /home/zach/Code/pico-debug/sdi12-analog-mux/calibration
./calibrate.py /dev/ttyUSB0 --read-nvm
```

This will:
1. Connect to the device
2. Send the `nvm_dump` command
3. Display all NVM contents
4. Exit without making any changes

**Example Output:**
```
================================================================================
NVM READ MODE - No calibration will be performed
================================================================================

=== Reading NVM from Device ===

NVM Contents:
================================================================================
========================================
NVM Data Dump
========================================

Header:
  Magic:    0x5344494D (valid)
  Version:  1
  CRC32:    0xABCD1234

Manufacturing:
  Date:     2025-11-24
  Serial:   DEVICE-12345
  Board:    UNKNOWN
  HW Ver:   1.0
  FW Ver:   1.0

Calibration:
  R_ref values (ohms):
    Ch0: 5031.62
    Ch1: 5031.62
    ...
================================================================================

✓ NVM read successfully!
```

**Use Cases:**
- Verify calibration after programming
- Check device information before deployment
- Document calibration values for records
- Troubleshoot calibration issues

## Troubleshooting

### Connection Issues

**Problem**: Cannot connect to device
```
ERROR: Failed to connect: [Errno 13] Permission denied: '/dev/ttyUSB0'
```

**Solution**:
```bash
# Add user to dialout group
sudo usermod -a -G dialout $USER
# Or temporarily:
sudo chmod 666 /dev/ttyUSB0
```

### Measurement Failures

**Problem**: Channel measurement returns "FAIL"
```
Time            Ch1     Ch2     Ch3     Ch4     Ch5     Ch6     Ch7
14:30:00        99.89   FAIL    99.81   99.68   99.85   99.67   99.79
```

**Solutions**:
- Check RTD connection for that channel
- Verify mux is switching properly
- Check ADC channel configuration
- Ensure reference resistor is connected

### High Standard Deviation

**Problem**: Standard deviation > 0.05 Ω
```
Channel | Ref Ω  | Measured Ω | Std Dev | Cal R_ref | Samples
  1     | 99.88  | 99.8765    | 0.1234  | 5032.15   | 20
```

**Causes**:
- Temperature drift in environment
- Poor electrical connections
- Electromagnetic interference
- Insufficient settling time

**Solutions**:
- Allow system to reach thermal equilibrium before calibrating
- Increase measurement duration (-d 30)
- Increase interval between measurements (-i 60)
- Check for loose connections

### Calibration Mode Not Working

**Problem**: Device doesn't enter calibration mode
```
ERROR: Not in calibration mode
```

**Solution**:
- Ensure firmware has calibration mode support (rebuild if needed)
- Send `cal_mode_start` command first
- Check UART terminal settings (115200 baud, 8N1)

### Calibration Values Out of Range

**Problem**: Calibrated R_ref values far from nominal (5030 Ω)
```
Channel | Cal R_ref
  1     | 5500.00     <-- Too high!
```

**Causes**:
- Reference resistor value entered incorrectly in script
- Wiring error (reversed connections)
- ADC configuration issue
- Incorrect 4-wire connection

**Solutions**:
- Verify reference resistor values with calibrated multimeter
- Check 4-wire connections (proper pairs: 2 current, 2 sense)
- Ensure script has correct reference resistor values
- Verify excitation current is correct (50 µA)

## Manual Calibration (Without Script)

If the Python script is unavailable, you can calibrate manually:

1. **Enter calibration mode**:
   ```
   cal_mode_start
   ```

2. **Set device information**:
   ```
   cal_set_serial DEVICE-12345
   cal_set_hw_ver 1.0
   cal_set_fw_ver 1.0
   cal_set_date 2025-11-24
   ```

3. **Collect measurements** (manually record values):
   ```
   cal_measure 1
   # Record resistance value
   cal_measure 2
   # Record resistance value
   ...
   ```

4. **Calculate R_ref** for each channel:
   ```
   R_ref_calibrated = R_ref_nominal * (R_actual / R_measured)

   Example:
   R_ref_nominal = 5030 Ω
   R_actual = 99.88 Ω (reference resistor)
   R_measured = 99.76 Ω (measured value)

   R_ref_calibrated = 5030 * (99.88 / 99.76) = 5036.05 Ω
   ```

5. **Write calibration values**:
   ```
   nvm_cal_rref 0 5036.05
   nvm_cal_rref 1 5035.23
   ...
   ```

6. **Exit calibration mode**:
   ```
   cal_mode_stop
   ```

## Best Practices

1. **Environment Stability**:
   - Allow 15+ minutes for system to reach thermal equilibrium
   - Perform calibration in stable room temperature environment
   - Avoid calibrating near heat sources or HVAC vents

2. **Electrical Connections**:
   - Use short, twisted-pair wires for RTD connections
   - Verify 4-wire configuration (2 current, 2 sense)
   - Check for oxidation or corrosion on connectors

3. **Measurement Duration**:
   - Minimum 10 minutes for production calibration
   - 20-30 measurements per channel recommended
   - Longer duration improves accuracy by averaging out noise

4. **Quality Control**:
   - Save calibration reports for traceability
   - Record ambient conditions (temperature, humidity)
   - Test device after calibration before deployment
   - Maintain calibration schedule (annual recommended)

5. **Documentation**:
   - Label device with serial number
   - Record calibration date on device
   - Store calibration reports in quality system
   - Track reference resistor calibration certificates

## Calibration Certificate Template

```
CALIBRATION CERTIFICATE

Device Information:
  Serial Number:     DEVICE-12345
  Hardware Version:  1.0
  Firmware Version:  1.0
  Calibration Date:  2025-11-24

Reference Standards:
  Standard:          Precision reference resistors (measured with calibrated multimeter)
  Certificate No:    REF-2025-001
  Uncertainty:       ±0.01 Ω

Calibration Results (per channel):
  Channel 1:  5032.15 Ω  (Std Dev: 0.0234 Ω, n=20)
  Channel 2:  5031.89 Ω  (Std Dev: 0.0189 Ω, n=20)
  Channel 3:  5030.98 Ω  (Std Dev: 0.0211 Ω, n=20)
  Channel 4:  5031.24 Ω  (Std Dev: 0.0245 Ω, n=20)
  Channel 5:  5032.42 Ω  (Std Dev: 0.0198 Ω, n=20)
  Channel 6:  5031.08 Ω  (Std Dev: 0.0223 Ω, n=20)
  Channel 7:  5031.56 Ω  (Std Dev: 0.0206 Ω, n=20)

Averaged Calibration (written to device):
  R_ref = 5031.62 Ω (Std Dev: 0.48 Ω)
  Applied to all 7 channels

Performed By:          [Technician Name]
Reviewed By:           [QC Manager Name]
Next Calibration Due:  2026-11-24
```

## Appendix: Firmware Commands Reference

### Calibration Mode Commands
| Command | Description | Example |
|---------|-------------|---------|
| `cal_mode_start` | Enter calibration mode | `cal_mode_start` |
| `cal_mode_stop` | Exit calibration mode | `cal_mode_stop` |
| `cal_set_serial <sn>` | Set device serial number | `cal_set_serial DEVICE-12345` |
| `cal_set_hw_ver <ver>` | Set hardware version | `cal_set_hw_ver 1.0` |
| `cal_set_fw_ver <ver>` | Set firmware version | `cal_set_fw_ver 1.0` |
| `cal_set_date <date>` | Set manufacturing date | `cal_set_date 2025-11-24` |
| `cal_measure <ch>` | Measure RTD channel (1-7) | `cal_measure 1` |

### NVM Commands
| Command | Description | Example |
|---------|-------------|---------|
| `nvm_dump` | Display all NVM data | `nvm_dump` |
| `nvm_reset` | Factory reset NVM | `nvm_reset` |
| `nvm_cal_rref <ch> <val>` | Set R_ref calibration | `nvm_cal_rref 0 5032.15` |
| `nvm_set_sn <sn>` | Set serial number | `nvm_set_sn DEVICE-12345` |
| `nvm_set_hw_ver <ver>` | Set HW version | `nvm_set_hw_ver 1.0` |
| `nvm_set_fw_ver <ver>` | Set FW version | `nvm_set_fw_ver 1.0` |
