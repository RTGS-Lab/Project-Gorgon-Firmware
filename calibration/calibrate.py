#!/usr/bin/env python3
"""
RTD Calibration Script for SDI-12 Analog Multiplexer
Collects resistance measurements over 10 minutes and calculates calibrated R_ref values
"""

import serial
import time
import sys
import argparse
from datetime import datetime
from dataclasses import dataclass
from typing import List
import statistics

@dataclass
class CalibrationData:
    """Holds calibration data for a single RTD channel"""
    channel: int
    reference_resistance: float  # Known reference resistance at 0°C
    measurements: List[float]
    current_rref: float = 5030.0  # Current R_ref from device NVM

    @property
    def mean_resistance(self) -> float:
        """Calculate mean of all measurements"""
        return statistics.mean(self.measurements) if self.measurements else 0.0

    @property
    def std_dev(self) -> float:
        """Calculate standard deviation of measurements"""
        return statistics.stdev(self.measurements) if len(self.measurements) > 1 else 0.0

    @property
    def calibrated_rref(self) -> float:
        """Calculate calibrated R_ref value

        The AD7124 measures resistance ratiometrically:
        R_RTD = (ADC_RTD / ADC_REF) * R_ref

        If we know the true R_RTD and measure ADC values, we can back-calculate R_ref:
        R_ref_calibrated = R_RTD_actual * (ADC_REF / ADC_RTD)

        Since the firmware already calculates resistance using the stored R_ref,
        we use that value (read from device NVM) for accurate calibration:
        R_ref_calibrated = R_ref_current * (R_RTD_actual / R_measured)
        """
        if self.mean_resistance == 0:
            return self.current_rref  # Return current value if no measurements

        # Use the current R_ref from device NVM (not a hardcoded value)
        # This is what the firmware used to calculate the measured resistance
        correction_factor = self.reference_resistance / self.mean_resistance
        calibrated = self.current_rref * correction_factor

        return calibrated


class CalibrationSession:
    """Manages the calibration session"""

    # Known reference resistances for channels 1-7 (PT100 at 0°C)
    REFERENCE_RESISTANCES = {
        1: 99.82,
        2: 99.80,
        3: 99.85,
        4: 99.88,
        5: 99.87,
        6: 99.81,
        7: 99.76,
    }

    def __init__(self, port: str, baudrate: int = 115200):
        self.port = port
        self.baudrate = baudrate
        self.serial = None
        self.calibration_data = {}
        self.average_r_ref = 5030.0  # Default value, will be calculated
        self.current_rref = 5030.0   # Current R_ref from device, read before calibration
        self.serial_number = None    # Device serial number, set during calibration

        # Initialize calibration data for all channels
        for channel, ref_resistance in self.REFERENCE_RESISTANCES.items():
            self.calibration_data[channel] = CalibrationData(
                channel=channel,
                reference_resistance=ref_resistance,
                measurements=[]
            )

    def set_current_rref(self, rref: float):
        """Set the current R_ref value for all channels"""
        self.current_rref = rref
        for channel in self.calibration_data:
            self.calibration_data[channel].current_rref = rref

    def connect(self):
        """Connect to the device"""
        print(f"Connecting to {self.port} at {self.baudrate} baud...")
        try:
            self.serial = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=1.0,
                write_timeout=1.0
            )
            time.sleep(2)  # Wait for device to reset
            print("Connected successfully!")
            return True
        except serial.SerialException as e:
            print(f"ERROR: Failed to connect: {e}")
            return False

    def disconnect(self):
        """Disconnect from the device"""
        if self.serial and self.serial.is_open:
            self.serial.close()
            print("Disconnected.")

    def send_command(self, command: str):
        """Send a command to the device"""
        if not self.serial or not self.serial.is_open:
            print("ERROR: Not connected")
            return

        cmd = command.strip() + '\n'
        self.serial.write(cmd.encode('utf-8'))
        self.serial.flush()

    def read_line(self, timeout: float = 1.0) -> str:
        """Read a line from the device"""
        if not self.serial or not self.serial.is_open:
            return ""

        self.serial.timeout = timeout
        try:
            line = self.serial.readline().decode('utf-8', errors='ignore').strip()
            return line
        except:
            return ""

    def flush_input(self):
        """Flush input buffer"""
        if self.serial and self.serial.is_open:
            self.serial.reset_input_buffer()

    def enter_calibration_mode(self, serial_number: str, hw_version: str, fw_version: str, board: str = "GORGON"):
        """Enter calibration mode on the device"""
        self.serial_number = serial_number  # Store for report
        print("\n=== Entering Calibration Mode ===")
        print(f"Serial Number: {serial_number}")
        print(f"Hardware Version: {hw_version}")
        print(f"Firmware Version: {fw_version}")
        print(f"Board: {board}")

        # Send calibration mode command
        self.send_command("cal_mode_start")
        time.sleep(0.5)

        # Clear any pending output
        self.flush_input()
        time.sleep(0.5)

        # Send device information
        self.send_command(f"cal_set_serial {serial_number}")
        time.sleep(0.2)

        self.send_command(f"cal_set_hw_ver {hw_version}")
        time.sleep(0.2)

        self.send_command(f"cal_set_fw_ver {fw_version}")
        time.sleep(0.2)

        # Set board name
        self.send_command(f"nvm_set_board {board}")
        time.sleep(0.2)

        # Get manufacturing date/time from system
        mfg_date = datetime.now().strftime("%Y-%m-%d")
        self.send_command(f"cal_set_date {mfg_date}")
        time.sleep(0.2)

        print("Calibration mode started successfully!")

    def measure_channel(self, channel: int) -> tuple:
        """Measure a single channel and return (resistance, temperature)"""
        # Request measurement via SDI-12 command emulation
        self.send_command(f"cal_measure {channel}")

        # Wait for measurement to complete
        time.sleep(0.5)

        # Read response lines looking for resistance data
        resistance = None
        temperature = None

        for _ in range(20):  # Read up to 20 lines
            line = self.read_line(timeout=0.5)
            if not line:
                continue

            # Parse output like: "RTD 1: 99.88Ω, 0.02°C (Raw: 0x123456)"
            if f"RTD {channel}:" in line:
                try:
                    # Extract resistance (before Ω symbol)
                    if 'Ω' in line:
                        parts = line.split('Ω')[0]
                        resistance_str = parts.split(':')[-1].strip()
                        resistance = float(resistance_str)

                    # Extract temperature (before °C symbol)
                    if '°C' in line:
                        parts = line.split('°C')[0]
                        temp_parts = parts.split(',')
                        if len(temp_parts) >= 2:
                            temperature = float(temp_parts[-1].strip())

                    if resistance is not None:
                        break
                except (ValueError, IndexError) as e:
                    print(f"Warning: Failed to parse line: {line} ({e})")
                    continue

        return (resistance, temperature)

    def collect_measurements(self, duration_minutes: int = 10, interval_seconds: int = 30):
        """Collect measurements over specified duration"""
        print(f"\n=== Starting Data Collection ===")
        print(f"Duration: {duration_minutes} minutes")
        print(f"Measurement interval: {interval_seconds} seconds")
        print(f"Channels: {list(self.REFERENCE_RESISTANCES.keys())}")

        start_time = time.time()
        end_time = start_time + (duration_minutes * 60)
        measurement_count = 0

        print("\nTime\t\tCh1\tCh2\tCh3\tCh4\tCh5\tCh6\tCh7")
        print("-" * 80)

        while time.time() < end_time:
            cycle_start = time.time()
            timestamp = datetime.now().strftime("%H:%M:%S")

            resistances = []

            # Measure all channels
            for channel in sorted(self.REFERENCE_RESISTANCES.keys()):
                resistance, temperature = self.measure_channel(channel)

                if resistance is not None:
                    self.calibration_data[channel].measurements.append(resistance)
                    resistances.append(f"{resistance:.2f}")
                else:
                    resistances.append("FAIL")
                    print(f"\nWarning: Failed to measure channel {channel}")

            # Print progress
            print(f"{timestamp}\t" + "\t".join(resistances))
            measurement_count += 1

            # Calculate time remaining
            elapsed = time.time() - start_time
            remaining = end_time - time.time()

            if remaining > 0:
                # Wait for next interval (accounting for measurement time)
                cycle_time = time.time() - cycle_start
                sleep_time = max(0, interval_seconds - cycle_time)

                if sleep_time > 0:
                    time.sleep(sleep_time)

        print("-" * 80)
        print(f"\nData collection complete! {measurement_count} measurement cycles collected.")

    def calculate_calibration(self):
        """Calculate calibrated R_ref values from collected data"""
        print("\n=== Calibration Results ===\n")
        print(f"Using current R_ref from device: {self.current_rref:.2f} Ω\n")

        print("Channel | Ref Ω  | Measured Ω | Std Dev | Cal R_ref | Samples")
        print("-" * 70)

        # Calculate individual R_ref for each channel
        r_ref_values = []
        for channel in sorted(self.calibration_data.keys()):
            data = self.calibration_data[channel]

            if len(data.measurements) == 0:
                print(f"  {channel}     | {data.reference_resistance:.2f}  | NO DATA    | -       | -         | 0")
                continue

            print(f"  {channel}     | {data.reference_resistance:.2f}  | "
                  f"{data.mean_resistance:.4f}    | {data.std_dev:.4f}  | "
                  f"{data.calibrated_rref:.2f}   | {len(data.measurements)}")

            r_ref_values.append(data.calibrated_rref)

        # Calculate average R_ref across all channels
        if r_ref_values:
            avg_r_ref = statistics.mean(r_ref_values)
            std_dev_r_ref = statistics.stdev(r_ref_values) if len(r_ref_values) > 1 else 0.0

            print("-" * 70)
            print(f"\nAverage Calibrated R_ref: {avg_r_ref:.2f} Ω")
            print(f"Standard Deviation:       {std_dev_r_ref:.2f} Ω")
            print(f"Channels Used:            {len(r_ref_values)}")

            # Store the average for later use
            self.average_r_ref = avg_r_ref
        else:
            print("\nERROR: No valid calibration data collected")
            self.average_r_ref = 5030.0  # Default

        print()

    def write_calibration_to_device(self):
        """Write averaged calibrated R_ref value to device NVM"""
        print("\n=== Writing Calibration to Device ===")
        print(f"Using averaged R_ref: {self.average_r_ref:.2f} Ω")
        print("Writing to all channels...\n")

        # Write the averaged R_ref to all 7 channels (0-6 in firmware)
        for firmware_channel in range(7):
            cmd = f"nvm_cal_rref {firmware_channel} {self.average_r_ref:.2f}"
            print(f"Channel {firmware_channel}: {cmd}")
            self.send_command(cmd)
            time.sleep(0.3)

            # Read response
            for _ in range(5):
                line = self.read_line(timeout=0.2)
                if line:
                    print(f"  -> {line}")

        print(f"\nCalibration complete! R_ref = {self.average_r_ref:.2f} Ω written to all channels.")

    def save_calibration_report(self, filename: str = None):
        """Save calibration report to file"""
        if filename is None:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            sn = self.serial_number or "unknown"
            filename = f"calibration_report_{sn}_{timestamp}.txt"

        filepath = f"/home/zach/Code/pico-debug/sdi12-analog-mux/calibration/{filename}"

        with open(filepath, 'w') as f:
            f.write("=" * 80 + "\n")
            f.write("RTD Calibration Report\n")
            f.write("=" * 80 + "\n\n")
            f.write(f"Serial Number: {self.serial_number}\n")
            f.write(f"Date: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"Port: {self.port}\n")
            f.write(f"Baudrate: {self.baudrate}\n\n")

            f.write("Calibration Results:\n")
            f.write("-" * 80 + "\n")
            f.write("Channel | Ref Ω  | Measured Ω | Std Dev | Cal R_ref | Samples\n")
            f.write("-" * 80 + "\n")

            for channel in sorted(self.calibration_data.keys()):
                data = self.calibration_data[channel]

                if len(data.measurements) == 0:
                    f.write(f"  {channel}     | {data.reference_resistance:.2f}  | NO DATA    | -       | -         | 0\n")
                    continue

                f.write(f"  {channel}     | {data.reference_resistance:.2f}  | "
                       f"{data.mean_resistance:.4f}    | {data.std_dev:.4f}  | "
                       f"{data.calibrated_rref:.2f}   | {len(data.measurements)}\n")

            f.write("-" * 80 + "\n")
            f.write(f"\nAVERAGED CALIBRATION VALUE (written to device):\n")
            f.write(f"  R_ref = {self.average_r_ref:.2f} Ω\n")
            f.write(f"  Applied to all 7 channels\n")

            f.write("\n" + "=" * 80 + "\n")
            f.write("Raw Measurements:\n")
            f.write("=" * 80 + "\n\n")

            for channel in sorted(self.calibration_data.keys()):
                data = self.calibration_data[channel]
                f.write(f"Channel {channel}:\n")
                f.write(f"  Reference: {data.reference_resistance} Ω\n")
                f.write(f"  Measurements: {data.measurements}\n")
                f.write(f"  Mean: {data.mean_resistance:.4f} Ω\n")
                f.write(f"  Std Dev: {data.std_dev:.4f} Ω\n")
                f.write(f"  Calibrated R_ref: {data.calibrated_rref:.2f} Ω\n\n")

        print(f"\nCalibration report saved to: {filepath}")

    def exit_calibration_mode(self):
        """Exit calibration mode"""
        print("\n=== Exiting Calibration Mode ===")
        self.send_command("cal_mode_stop")
        time.sleep(0.5)

        # Read any responses
        for _ in range(5):
            line = self.read_line(timeout=0.2)
            if line:
                print(f"  {line}")

    def read_nvm(self):
        """Read and display current NVM contents from device"""
        print("\n=== Reading NVM from Device ===")

        # Flush any pending output
        self.flush_input()
        time.sleep(0.2)

        # Send nvm_dump command
        self.send_command("nvm_dump")
        time.sleep(0.5)

        # Read all output (should be ~30-40 lines)
        print("\nNVM Contents:")
        print("=" * 80)

        lines_read = 0
        empty_count = 0
        max_empty = 3  # Stop after 3 consecutive empty reads

        while empty_count < max_empty and lines_read < 100:
            line = self.read_line(timeout=0.3)
            if line:
                print(line)
                lines_read += 1
                empty_count = 0
            else:
                empty_count += 1

        print("=" * 80)
        print(f"\nRead {lines_read} lines from device NVM")

        return lines_read > 0

    def read_current_rref(self):
        """Read current R_ref calibration value from device NVM.

        All channels share the same R_ref, so we just read Ch0's value.
        Returns the R_ref value, or 5030.0 as default on failure.
        """
        # Flush any pending output
        self.flush_input()
        time.sleep(0.2)

        # Send nvm_dump command
        self.send_command("nvm_dump")
        time.sleep(0.5)

        # Read output and find first R_ref value
        empty_count = 0
        max_empty = 3

        while empty_count < max_empty:
            line = self.read_line(timeout=0.3)
            if line:
                empty_count = 0
                # Parse line like "    Ch0: 5030.50"
                if "Ch0:" in line:
                    try:
                        val_part = line.split(":")[1].strip()
                        rref = float(val_part)
                        print(f"Current R_ref from device: {rref:.2f} Ω")
                        # Drain remaining output
                        while self.read_line(timeout=0.1):
                            pass
                        return rref
                    except (ValueError, IndexError):
                        pass
            else:
                empty_count += 1

        print("Warning: Could not read R_ref from device, using default 5030.0 Ω")
        return 5030.0


def main():
    parser = argparse.ArgumentParser(
        description='RTD Calibration Tool for SDI-12 Analog Multiplexer'
    )
    parser.add_argument('port', help='Serial port (e.g., /dev/ttyUSB0 or COM3)')
    parser.add_argument('-b', '--baudrate', type=int, default=115200,
                       help='Baud rate (default: 115200)')
    parser.add_argument('-d', '--duration', type=int, default=10,
                       help='Calibration duration in minutes (default: 10)')
    parser.add_argument('-i', '--interval', type=int, default=30,
                       help='Measurement interval in seconds (default: 30)')
    parser.add_argument('-s', '--serial',
                       help='Device serial number (required for calibration)')
    parser.add_argument('--hw-version', default='1.0',
                       help='Hardware version (default: 1.0)')
    parser.add_argument('--fw-version', default='1.0',
                       help='Firmware version (default: 1.0)')
    parser.add_argument('--board', default='GORGON',
                       help='Board name (default: GORGON)')
    parser.add_argument('--no-write', action='store_true',
                       help='Skip writing calibration to device (dry run)')
    parser.add_argument('--read-nvm', action='store_true',
                       help='Read and display NVM contents from device (no calibration)')
    parser.add_argument('--set-rref', type=float,
                       help='Directly set R_ref value (ohms) to all channels and save')

    args = parser.parse_args()

    # Check if serial number is required (not needed for read-nvm or set-rref)
    if not args.read_nvm and not args.set_rref and not args.serial:
        parser.error("--serial is required for calibration (use --read-nvm or --set-rref for quick operations)")

    # Create calibration session
    session = CalibrationSession(port=args.port, baudrate=args.baudrate)

    try:
        # Connect to device
        if not session.connect():
            return 1

        # If --read-nvm flag is set, just read NVM and exit
        if args.read_nvm:
            print("\n" + "=" * 80)
            print("NVM READ MODE - No calibration will be performed")
            print("=" * 80)

            if session.read_nvm():
                print("\n✓ NVM read successfully!")
                return 0
            else:
                print("\n✗ Failed to read NVM")
                return 1

        # If --set-rref flag is set, directly set R_ref and exit
        if args.set_rref:
            print("\n" + "=" * 80)
            print(f"SET R_REF MODE - Setting R_ref to {args.set_rref:.2f} Ω")
            print("=" * 80)

            # Flush and wait for device to be ready
            session.flush_input()
            time.sleep(0.5)

            # Write R_ref to all 7 channels
            for ch in range(7):
                cmd = f"nvm_cal_rref {ch} {args.set_rref:.2f}"
                print(f"  {cmd}")
                session.send_command(cmd)
                time.sleep(0.3)
                # Read any response
                for _ in range(3):
                    line = session.read_line(timeout=0.1)
                    if line:
                        print(f"    {line}")

            # Save NVM
            print("\nSaving NVM...")
            session.send_command("nvm_save")
            time.sleep(0.5)

            # Read response
            for _ in range(10):
                line = session.read_line(timeout=0.2)
                if line:
                    print(f"  {line}")

            print(f"\n✓ R_ref set to {args.set_rref:.2f} Ω on all channels!")
            return 0

        # Normal calibration mode
        # Read current R_ref from device before calibration
        # This is critical for accurate calibration calculations
        current_rref = session.read_current_rref()
        session.set_current_rref(current_rref)

        # Enter calibration mode
        session.enter_calibration_mode(
            serial_number=args.serial,
            hw_version=args.hw_version,
            fw_version=args.fw_version,
            board=args.board
        )

        # Collect measurements
        session.collect_measurements(
            duration_minutes=args.duration,
            interval_seconds=args.interval
        )

        # Calculate calibration
        session.calculate_calibration()

        # Write to device
        if not args.no_write:
            response = input("\nWrite calibration to device NVM? (yes/no): ")
            if response.lower() in ['yes', 'y']:
                session.write_calibration_to_device()
                print("\n✓ Calibration complete!")
            else:
                print("\nCalibration NOT written to device.")
        else:
            print("\nDry run mode - calibration NOT written to device.")

        # Save report
        session.save_calibration_report()

        # Exit calibration mode
        session.exit_calibration_mode()

        return 0

    except KeyboardInterrupt:
        print("\n\nCalibration interrupted by user.")
        return 1

    except Exception as e:
        print(f"\nERROR: {e}")
        import traceback
        traceback.print_exc()
        return 1

    finally:
        session.disconnect()


if __name__ == '__main__':
    sys.exit(main())
