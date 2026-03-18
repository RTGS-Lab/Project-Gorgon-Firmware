#!/usr/bin/env python3
"""
RTD Serial Data Plotter
Reads resistance values from Pico RTD measurement system and plots them live.
"""

import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import re
import time
from collections import deque
import argparse

class RTDPlotter:
    def __init__(self, port, baudrate=115200, max_points=500):
        self.port = port
        self.baudrate = baudrate
        self.max_points = max_points

        # Data storage
        self.times = deque(maxlen=max_points)
        self.resistances = deque(maxlen=max_points)
        self.temperatures = deque(maxlen=max_points)

        # Serial connection
        self.serial_conn = None
        self.start_time = time.time()

        # Regex patterns to extract data
        self.resistance_pattern = re.compile(r'RTD resistance calculated: ([\d.]+) ohms')
        self.temperature_pattern = re.compile(r'Temperature calculated: ([\d.-]+)')

        # Update tracking
        self.last_update_time = 0
        self.update_counter = 0

        # Setup matplotlib
        self.setup_plot()

    def setup_plot(self):
        """Setup the matplotlib figure and subplots"""
        plt.style.use('dark_background')
        self.fig, (self.ax1, self.ax2) = plt.subplots(2, 1, figsize=(12, 8))
        self.fig.suptitle('Live RTD Measurements', fontsize=16, color='white')

        # Resistance plot
        self.line1, = self.ax1.plot([], [], 'cyan', linewidth=2, label='Resistance (Ω)')
        self.ax1.set_ylabel('Resistance (Ω)', color='cyan')
        self.ax1.tick_params(axis='y', labelcolor='cyan')
        self.ax1.grid(True, alpha=0.3)
        self.ax1.legend()

        # Temperature plot
        self.line2, = self.ax2.plot([], [], 'orange', linewidth=2, label='Temperature (°C)')
        self.ax2.set_xlabel('Time (s)')
        self.ax2.set_ylabel('Temperature (°C)', color='orange')
        self.ax2.tick_params(axis='y', labelcolor='orange')
        self.ax2.grid(True, alpha=0.3)
        self.ax2.legend()

        plt.tight_layout()

    def connect_serial(self):
        """Connect to the serial port"""
        try:
            self.serial_conn = serial.Serial(self.port, self.baudrate, timeout=0.01)  # Very short timeout
            print(f"Connected to {self.port} at {self.baudrate} baud")
            time.sleep(1)  # Shorter connection stabilization
            return True
        except serial.SerialException as e:
            print(f"Failed to connect to {self.port}: {e}")
            return False

    def read_serial_data(self):
        """Read and parse data from serial port"""
        if not self.serial_conn or not self.serial_conn.is_open:
            return None, None

        resistances = []
        timestamps = []

        try:
            # Read multiple lines in one go for faster processing
            while self.serial_conn.in_waiting > 0:
                line = self.serial_conn.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    # Look for resistance values
                    resistance_match = self.resistance_pattern.search(line)
                    if resistance_match:
                        resistance = float(resistance_match.group(1))
                        current_time = time.time() - self.start_time
                        resistances.append(resistance)
                        timestamps.append(current_time)

        except (serial.SerialException, UnicodeDecodeError, ValueError) as e:
            print(f"Serial read error: {e}")

        # Return the latest resistance value if any found
        if resistances:
            return resistances[-1], timestamps[-1]
        return None, None

    def update_plot(self, frame):
        """Animation update function"""
        resistance, timestamp = self.read_serial_data()

        if resistance is not None and timestamp is not None:
            # Add new data
            self.times.append(timestamp)
            self.resistances.append(resistance)

            # Calculate temperature (simple linear approximation for demo)
            # You can modify this to match your actual RTD calculation
            temp = (resistance - 100.0) / 0.385  # Simplified PT100 calculation
            self.temperatures.append(temp)

            self.update_counter += 1

            # Update plots only every few data points or when significant time has passed
            current_time = time.time()
            if (self.update_counter % 5 == 0 or
                current_time - self.last_update_time > 0.1):  # Update every 5 points or 100ms

                if len(self.times) > 1:
                    self.line1.set_data(list(self.times), list(self.resistances))
                    self.line2.set_data(list(self.times), list(self.temperatures))

                    # Auto-scale axes less frequently for performance
                    if self.update_counter % 20 == 0:  # Scale every 20 updates
                        self.ax1.relim()
                        self.ax1.autoscale_view()
                        self.ax2.relim()
                        self.ax2.autoscale_view()

                    # Update title with latest values
                    self.fig.suptitle(
                        f'Live RTD Measurements - R: {resistance:.2f}Ω, T: {temp:.1f}°C',
                        fontsize=16, color='white'
                    )

                self.last_update_time = current_time

        return self.line1, self.line2

    def start_plotting(self):
        """Start the live plotting"""
        if not self.connect_serial():
            return

        print("Starting live plot... Press Ctrl+C to stop")

        # Start animation with much faster interval
        ani = animation.FuncAnimation(
            self.fig, self.update_plot, interval=10, blit=False, cache_frame_data=False
        )

        try:
            plt.show()
        except KeyboardInterrupt:
            print("Stopping plotter...")
        finally:
            if self.serial_conn and self.serial_conn.is_open:
                self.serial_conn.close()
                print("Serial connection closed")

def main():
    parser = argparse.ArgumentParser(description='Plot live RTD data from serial port')
    parser.add_argument('--port', '-p', default='/dev/ttyACM0',
                       help='Serial port (default: /dev/ttyACM0)')
    parser.add_argument('--baud', '-b', type=int, default=115200,
                       help='Baud rate (default: 115200)')
    parser.add_argument('--points', '-n', type=int, default=500,
                       help='Maximum points to display (default: 500)')

    args = parser.parse_args()

    plotter = RTDPlotter(args.port, args.baud, args.points)
    plotter.start_plotting()

if __name__ == "__main__":
    main()