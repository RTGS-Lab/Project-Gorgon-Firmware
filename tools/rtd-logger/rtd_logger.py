#!/usr/bin/env python3
"""
RTD Data Logger
Reads resistance values from Pico RTD measurement system and logs them to CSV.
Designed to collect data over long periods for precision and accuracy analysis.
"""

import serial
import csv
import re
import time
import argparse
from datetime import datetime
import sys
import os


class RTDLogger:
    def __init__(self, port, baudrate=115200, output_dir='data'):
        self.port = port
        self.baudrate = baudrate
        self.output_dir = output_dir

        # Serial connection
        self.serial_conn = None
        self.start_time = time.time()

        # Regex pattern to extract RTD summary data
        # Matches: "Summary: RTD1=XXX.XXΩ, RTD2=YYY.YYΩ, RTD3=ZZZ.ZZΩ"
        self.summary_pattern = re.compile(
            r'Summary: RTD1=([\d.]+)Ω, RTD2=([\d.]+)Ω, RTD3=([\d.]+)Ω'
        )

        # Statistics tracking
        self.sample_count = 0
        self.last_print_time = time.time()

        # CSV file setup
        self.csv_file = None
        self.csv_writer = None

        # Ensure output directory exists
        os.makedirs(self.output_dir, exist_ok=True)

    def connect_serial(self):
        """Connect to the serial port"""
        try:
            self.serial_conn = serial.Serial(self.port, self.baudrate, timeout=1)
            print(f"Connected to {self.port} at {self.baudrate} baud")
            time.sleep(2)  # Wait for connection to stabilize
            return True
        except serial.SerialException as e:
            print(f"Failed to connect to {self.port}: {e}")
            return False

    def setup_csv(self):
        """Create CSV file with timestamp in filename"""
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        filename = os.path.join(self.output_dir, f'rtd_data_{timestamp}.csv')

        try:
            self.csv_file = open(filename, 'w', newline='')
            self.csv_writer = csv.writer(self.csv_file)

            # Write header
            self.csv_writer.writerow([
                'timestamp',
                'elapsed_time_s',
                'rtd1_ohms',
                'rtd2_ohms',
                'rtd3_ohms'
            ])
            self.csv_file.flush()

            print(f"Logging data to: {filename}")
            return True
        except Exception as e:
            print(f"Failed to create CSV file: {e}")
            return False

    def read_and_log_data(self):
        """Read data from serial port and log to CSV"""
        if not self.serial_conn or not self.serial_conn.is_open:
            return False

        try:
            # Read lines from serial port
            while self.serial_conn.in_waiting > 0:
                line = self.serial_conn.readline().decode('utf-8', errors='ignore').strip()

                if line:
                    # Look for summary line with all 3 RTD values
                    match = self.summary_pattern.search(line)

                    if match:
                        rtd1 = float(match.group(1))
                        rtd2 = float(match.group(2))
                        rtd3 = float(match.group(3))

                        # Record timestamp
                        timestamp = datetime.now().isoformat()
                        elapsed_time = time.time() - self.start_time

                        # Write to CSV
                        self.csv_writer.writerow([
                            timestamp,
                            f'{elapsed_time:.3f}',
                            f'{rtd1:.2f}',
                            f'{rtd2:.2f}',
                            f'{rtd3:.2f}'
                        ])
                        self.csv_file.flush()

                        self.sample_count += 1

                        # Print status every 10 seconds
                        current_time = time.time()
                        if current_time - self.last_print_time >= 10:
                            print(f"Samples: {self.sample_count:6d} | "
                                  f"Runtime: {elapsed_time:8.1f}s | "
                                  f"Latest: RTD1={rtd1:6.2f}Ω RTD2={rtd2:6.2f}Ω RTD3={rtd3:6.2f}Ω")
                            self.last_print_time = current_time

                        return True

        except (serial.SerialException, UnicodeDecodeError, ValueError) as e:
            print(f"Error reading serial data: {e}")
            return False

        return True

    def start_logging(self):
        """Start the data logging process"""
        if not self.connect_serial():
            return

        if not self.setup_csv():
            return

        print("\nRTD Data Logger Started")
        print("=" * 60)
        print("Press Ctrl+C to stop logging and view statistics")
        print("=" * 60)

        try:
            while True:
                self.read_and_log_data()
                time.sleep(0.01)  # Small delay to prevent busy-waiting

        except KeyboardInterrupt:
            print("\n\nStopping logger...")
            self.print_statistics()
        finally:
            self.cleanup()

    def print_statistics(self):
        """Print summary statistics"""
        elapsed = time.time() - self.start_time
        print("\n" + "=" * 60)
        print("LOGGING SESSION SUMMARY")
        print("=" * 60)
        print(f"Total samples collected: {self.sample_count}")
        print(f"Total runtime: {elapsed:.1f} seconds ({elapsed/60:.1f} minutes)")
        if self.sample_count > 0:
            print(f"Average sample rate: {self.sample_count/elapsed:.2f} samples/second")
        print(f"Data saved to: {self.csv_file.name if self.csv_file else 'N/A'}")
        print("=" * 60)

    def cleanup(self):
        """Clean up resources"""
        if self.csv_file:
            self.csv_file.close()
            print("CSV file closed")

        if self.serial_conn and self.serial_conn.is_open:
            self.serial_conn.close()
            print("Serial connection closed")


def main():
    parser = argparse.ArgumentParser(
        description='Log RTD resistance data from serial port to CSV',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --port /dev/ttyACM0
  %(prog)s --port /dev/ttyUSB0 --baud 115200 --output-dir ./measurements
        """
    )

    parser.add_argument('--port', '-p', default='/dev/ttyACM0',
                       help='Serial port (default: /dev/ttyACM0)')
    parser.add_argument('--baud', '-b', type=int, default=115200,
                       help='Baud rate (default: 115200)')
    parser.add_argument('--output-dir', '-o', default='data',
                       help='Output directory for CSV files (default: data)')

    args = parser.parse_args()

    logger = RTDLogger(args.port, args.baud, args.output_dir)
    logger.start_logging()


if __name__ == "__main__":
    main()
