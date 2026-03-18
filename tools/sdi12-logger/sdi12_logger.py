#!/usr/bin/env python3
"""
SDI-12 Temperature Logger
Continuously logs temperature data from SDI-12 sensor (M1-M9 commands)
Logs to CSV file with timestamps.
"""

import serial
import time
import csv
import argparse
import sys
from datetime import datetime
from pathlib import Path


class SDI12Logger:
    def __init__(self, port, address='0', baudrate=1200, log_file=None, interval=5.0):
        self.port = port
        self.address = address
        self.baudrate = baudrate
        self.interval = interval
        self.serial_conn = None

        # Setup log file
        if log_file is None:
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            log_file = f'sdi12_data_{timestamp}.csv'
        self.log_file = Path(log_file)

        # CSV headers
        self.headers = [
            'timestamp',
            'datetime',
            'M1_RTD1_temp_C',
            'M2_RTD2_temp_C',
            'M3_RTD3_temp_C',
            'M4_RTD4_temp_C',
            'M5_RTD5_temp_C',
            'M6_RTD6_temp_C',
            'M7_RTD7_temp_C',
            'M8_Pico_temp_C',
            'M9_ADC_temp_C'
        ]

        # Initialize log file
        self.init_log_file()

    def init_log_file(self):
        """Initialize CSV log file with headers"""
        with open(self.log_file, 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(self.headers)
        print(f"Log file created: {self.log_file}")

    def connect_serial(self):
        """Connect to the serial port (Adafruit Feather)"""
        try:
            # The Feather acts as a USB-serial bridge
            # Use higher baudrate for USB communication with Feather
            self.serial_conn = serial.Serial(self.port, 115200, timeout=2)
            time.sleep(2)  # Wait for connection to stabilize
            print(f"Connected to {self.port}")

            # Clear any pending data
            self.serial_conn.reset_input_buffer()
            return True
        except serial.SerialException as e:
            print(f"Failed to connect to {self.port}: {e}")
            return False

    def send_sdi12_command(self, command):
        """Send SDI-12 command via Feather and return response"""
        if not self.serial_conn or not self.serial_conn.is_open:
            return None

        try:
            # Clear buffers
            self.serial_conn.reset_input_buffer()

            # Send command
            cmd = f"{command}\n"
            self.serial_conn.write(cmd.encode())
            print(f"  Sent: {command}")

            # Wait for response
            time.sleep(0.2)

            # Read response
            response_lines = []
            start_time = time.time()
            while time.time() - start_time < 1.0:  # 1 second timeout
                if self.serial_conn.in_waiting > 0:
                    line = self.serial_conn.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        response_lines.append(line)
                        print(f"  Recv: {line}")
                time.sleep(0.01)

            return response_lines

        except Exception as e:
            print(f"  Error sending command: {e}")
            return None

    def parse_measurement_response(self, response_lines):
        """Parse SDI-12 measurement response to extract temperature value"""
        # Look for response format: a±x.xxx±y.yyy<CR><LF>
        # Example: "0+26.31" or "0-5.23"
        # Need to skip status responses like "00001" (atttn format)
        import re

        for line in response_lines:
            # Skip echo lines (lines starting with '>')
            if line.startswith('>'):
                continue

            # Look for lines starting with our address
            if line and len(line) > 1 and line[0] == self.address:
                # Remove address character
                data = line[1:].strip()

                # Skip measurement status responses (atttn format: 5 digits)
                # Example: "0001" means 000 seconds, 1 value
                if re.match(r'^\d{4,5}$', data):
                    print(f"  Skipping status line: {line}")
                    continue

                # Match signed floating point number at the start
                # Must have either a decimal point or a sign to be valid data
                match = re.match(r'([+-]\d+\.?\d*|\d+\.\d+)', data)
                if match:
                    try:
                        value = float(match.group(1))
                        print(f"  Parsed value: {value}")
                        return value
                    except ValueError:
                        print(f"  Failed to convert '{match.group(1)}' to float")
                        pass

        print(f"  Could not find valid data in response: {response_lines}")
        return None

    def measure_sensor(self, measurement_num):
        """
        Perform a complete SDI-12 measurement cycle for a specific sensor
        measurement_num: 1-9 (M1-M9 commands)
        Returns: temperature value or None
        """
        print(f"\nMeasuring M{measurement_num}...")

        # Step 1: Send measurement command (e.g., "0M1!")
        measure_cmd = f"{self.address}M{measurement_num}!"
        measure_response = self.send_sdi12_command(measure_cmd)

        if not measure_response:
            print(f"  No response to M{measurement_num} command")
            return None

        # Step 2: Parse the measurement response (atttn format)
        # Response should be like "00001" (000 seconds delay, 1 value)
        # Wait the indicated time for measurement to complete
        time.sleep(1.0)  # Increased wait time for measurement to complete

        # Step 3: Send data request command (e.g., "0D0!")
        data_cmd = f"{self.address}D0!"
        data_response = self.send_sdi12_command(data_cmd)

        if not data_response:
            print(f"  No response to D0 command")
            return None

        # Step 4: Parse the data response
        # Debug: show all response lines
        print(f"  D0 response lines: {data_response}")

        temperature = self.parse_measurement_response(data_response)
        if temperature is not None:
            print(f"  Temperature: {temperature:.2f}°C")
        else:
            print(f"  Failed to parse temperature")

        return temperature

    def log_all_measurements(self):
        """Measure all sensors (M1-M9) and log to CSV"""
        timestamp = time.time()
        dt = datetime.now().strftime('%Y-%m-%d %H:%M:%S')

        print(f"\n{'='*60}")
        print(f"Logging cycle: {dt}")
        print(f"{'='*60}")

        # Measure all sensors
        measurements = {
            'timestamp': timestamp,
            'datetime': dt,
            'M1_RTD1_temp_C': self.measure_sensor(1),
            'M2_RTD2_temp_C': self.measure_sensor(2),
            'M3_RTD3_temp_C': self.measure_sensor(3),
            'M4_RTD4_temp_C': self.measure_sensor(4),
            'M5_RTD5_temp_C': self.measure_sensor(5),
            'M6_RTD6_temp_C': self.measure_sensor(6),
            'M7_RTD7_temp_C': self.measure_sensor(7),
            'M8_Pico_temp_C': self.measure_sensor(8),
            'M9_ADC_temp_C': self.measure_sensor(9),
        }

        # Write to CSV
        with open(self.log_file, 'a', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=self.headers)
            writer.writerow(measurements)

        print(f"\nData logged to {self.log_file}")
        print(f"{'='*60}\n")

        return measurements

    def run(self):
        """Main logging loop"""
        if not self.connect_serial():
            return

        print(f"\nSDI-12 Logger Started")
        print(f"  Port: {self.port}")
        print(f"  SDI-12 Address: {self.address}")
        print(f"  Log File: {self.log_file}")
        print(f"  Interval: {self.interval} seconds")
        print(f"\nPress Ctrl+C to stop\n")

        cycle_count = 0

        try:
            while True:
                cycle_count += 1
                print(f"\n>>> Cycle {cycle_count} <<<")

                self.log_all_measurements()

                print(f"Waiting {self.interval} seconds until next cycle...")
                time.sleep(self.interval)

        except KeyboardInterrupt:
            print("\n\nLogging stopped by user")
        except Exception as e:
            print(f"\nError during logging: {e}")
            import traceback
            traceback.print_exc()
        finally:
            if self.serial_conn and self.serial_conn.is_open:
                self.serial_conn.close()
                print("Serial connection closed")
            print(f"\nData saved to: {self.log_file}")


def main():
    parser = argparse.ArgumentParser(
        description='Log SDI-12 temperature data continuously to CSV',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Log with default settings (5 second interval)
  ./sdi12_logger.py --port /dev/ttyACM0

  # Log every 10 seconds with custom filename
  ./sdi12_logger.py --port /dev/ttyACM0 --interval 10 --output temps.csv

  # Use different SDI-12 address
  ./sdi12_logger.py --port /dev/ttyACM0 --address 1
        """
    )

    parser.add_argument('--port', '-p', required=True,
                       help='Serial port for Adafruit Feather (e.g., /dev/ttyACM0)')
    parser.add_argument('--address', '-a', default='0',
                       help='SDI-12 sensor address (default: 0)')
    parser.add_argument('--baud', '-b', type=int, default=1200,
                       help='SDI-12 baud rate (default: 1200, not used for USB)')
    parser.add_argument('--interval', '-i', type=float, default=5.0,
                       help='Logging interval in seconds (default: 5.0)')
    parser.add_argument('--output', '-o', default=None,
                       help='Output CSV file (default: auto-generated with timestamp)')

    args = parser.parse_args()

    logger = SDI12Logger(
        port=args.port,
        address=args.address,
        baudrate=args.baud,
        log_file=args.output,
        interval=args.interval
    )

    logger.run()


if __name__ == "__main__":
    main()
