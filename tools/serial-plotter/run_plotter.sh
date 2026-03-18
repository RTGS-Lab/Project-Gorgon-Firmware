#!/bin/bash

# RTD Plotter Runner Script
# This script helps run the RTD plotter with common configurations

echo "RTD Serial Plotter"
echo "=================="

# Check if Python is available
if ! command -v python3 &> /dev/null; then
    echo "Error: Python3 is not installed"
    exit 1
fi

# Check if dependencies are installed
echo "Checking dependencies..."
python3 -c "import serial, matplotlib, numpy" 2>/dev/null
if [ $? -ne 0 ]; then
    echo "Installing dependencies..."
    pip3 install -r requirements.txt
fi

# Find available serial ports
echo "Available serial ports:"
ls /dev/tty* 2>/dev/null | grep -E "(ttyACM|ttyUSB)" || echo "No USB/ACM ports found"

# Default port
DEFAULT_PORT="/dev/ttyACM0"

# Ask user for port if not specified
if [ -z "$1" ]; then
    echo
    read -p "Enter serial port (default: $DEFAULT_PORT): " USER_PORT
    PORT=${USER_PORT:-$DEFAULT_PORT}
else
    PORT=$1
fi

echo "Starting RTD plotter on port: $PORT"
echo "Press Ctrl+C to stop"
echo

# Run the plotter
python3 rtd_plotter.py --port "$PORT"