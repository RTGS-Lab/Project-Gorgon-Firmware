#!/usr/bin/env python3
"""
RTD Data Analysis Tool
Analyzes CSV data from rtd_logger.py to calculate precision and accuracy statistics.
"""

import csv
import argparse
import sys
import os
from statistics import mean, stdev, median
from datetime import datetime


class RTDAnalyzer:
    def __init__(self, csv_file):
        self.csv_file = csv_file
        self.data = {
            'timestamps': [],
            'elapsed_times': [],
            'rtd1': [],
            'rtd2': [],
            'rtd3': []
        }

    def load_data(self):
        """Load data from CSV file"""
        try:
            with open(self.csv_file, 'r') as f:
                reader = csv.DictReader(f)
                for row in reader:
                    self.data['timestamps'].append(row['timestamp'])
                    self.data['elapsed_times'].append(float(row['elapsed_time_s']))
                    self.data['rtd1'].append(float(row['rtd1_ohms']))
                    self.data['rtd2'].append(float(row['rtd2_ohms']))
                    self.data['rtd3'].append(float(row['rtd3_ohms']))

            sample_count = len(self.data['rtd1'])
            if sample_count == 0:
                print("Error: No data found in CSV file")
                return False

            print(f"Loaded {sample_count} samples from {self.csv_file}")
            return True

        except FileNotFoundError:
            print(f"Error: File not found: {self.csv_file}")
            return False
        except Exception as e:
            print(f"Error loading CSV: {e}")
            return False

    def calculate_statistics(self, data, label):
        """Calculate statistics for a single RTD"""
        n = len(data)
        if n == 0:
            return None

        mean_val = mean(data)
        median_val = median(data)
        min_val = min(data)
        max_val = max(data)
        range_val = max_val - min_val

        # Calculate standard deviation (only if n > 1)
        if n > 1:
            std_dev = stdev(data)
            # Calculate coefficient of variation (CV) as a measure of precision
            cv_percent = (std_dev / mean_val) * 100 if mean_val != 0 else 0
        else:
            std_dev = 0
            cv_percent = 0

        return {
            'label': label,
            'n': n,
            'mean': mean_val,
            'median': median_val,
            'std_dev': std_dev,
            'min': min_val,
            'max': max_val,
            'range': range_val,
            'cv_percent': cv_percent
        }

    def print_statistics(self):
        """Print comprehensive statistics for all RTDs"""
        print("\n" + "=" * 80)
        print("RTD MEASUREMENT STATISTICS")
        print("=" * 80)

        # File info
        print(f"\nFile: {self.csv_file}")
        print(f"Samples: {len(self.data['rtd1'])}")

        if len(self.data['elapsed_times']) > 0:
            duration = self.data['elapsed_times'][-1] - self.data['elapsed_times'][0]
            print(f"Duration: {duration:.1f} seconds ({duration/60:.1f} minutes)")
            if duration > 0:
                rate = len(self.data['rtd1']) / duration
                print(f"Sample rate: {rate:.3f} samples/second")

        # Calculate statistics for each RTD
        rtd_stats = []
        for rtd_num in range(1, 4):
            stats = self.calculate_statistics(
                self.data[f'rtd{rtd_num}'],
                f'RTD{rtd_num}'
            )
            if stats:
                rtd_stats.append(stats)

        # Print detailed statistics table
        print("\n" + "-" * 80)
        print(f"{'Metric':<20} {'RTD1':<18} {'RTD2':<18} {'RTD3':<18}")
        print("-" * 80)

        if rtd_stats:
            print(f"{'Mean (Ω)':<20} "
                  f"{rtd_stats[0]['mean']:>16.4f}  "
                  f"{rtd_stats[1]['mean']:>16.4f}  "
                  f"{rtd_stats[2]['mean']:>16.4f}")

            print(f"{'Median (Ω)':<20} "
                  f"{rtd_stats[0]['median']:>16.4f}  "
                  f"{rtd_stats[1]['median']:>16.4f}  "
                  f"{rtd_stats[2]['median']:>16.4f}")

            print(f"{'Std Dev (Ω)':<20} "
                  f"{rtd_stats[0]['std_dev']:>16.4f}  "
                  f"{rtd_stats[1]['std_dev']:>16.4f}  "
                  f"{rtd_stats[2]['std_dev']:>16.4f}")

            print(f"{'Min (Ω)':<20} "
                  f"{rtd_stats[0]['min']:>16.4f}  "
                  f"{rtd_stats[1]['min']:>16.4f}  "
                  f"{rtd_stats[2]['min']:>16.4f}")

            print(f"{'Max (Ω)':<20} "
                  f"{rtd_stats[0]['max']:>16.4f}  "
                  f"{rtd_stats[1]['max']:>16.4f}  "
                  f"{rtd_stats[2]['max']:>16.4f}")

            print(f"{'Range (Ω)':<20} "
                  f"{rtd_stats[0]['range']:>16.4f}  "
                  f"{rtd_stats[1]['range']:>16.4f}  "
                  f"{rtd_stats[2]['range']:>16.4f}")

            print(f"{'CV (%)':<20} "
                  f"{rtd_stats[0]['cv_percent']:>16.4f}  "
                  f"{rtd_stats[1]['cv_percent']:>16.4f}  "
                  f"{rtd_stats[2]['cv_percent']:>16.4f}")

        print("-" * 80)

        # Print precision analysis
        print("\n" + "=" * 80)
        print("PRECISION ANALYSIS")
        print("=" * 80)
        print("\nCoefficient of Variation (CV) indicates measurement precision:")
        print("  < 1%:  Excellent precision")
        print("  1-2%:  Good precision")
        print("  2-5%:  Moderate precision")
        print("  > 5%:  Poor precision")
        print()

        for stats in rtd_stats:
            cv = stats['cv_percent']
            if cv < 1:
                assessment = "Excellent"
            elif cv < 2:
                assessment = "Good"
            elif cv < 5:
                assessment = "Moderate"
            else:
                assessment = "Poor"

            print(f"{stats['label']}: CV = {cv:.4f}% ({assessment})")

        # Print accuracy notes
        print("\n" + "=" * 80)
        print("ACCURACY NOTES")
        print("=" * 80)
        print("To assess accuracy, compare the mean resistance values to known reference:")
        print("  - For PT100 at 0°C: Expected ~100.00Ω")
        print("  - For other temperatures: Use PT100 resistance-temperature tables")
        print("  - Accuracy (%) = |Mean - Reference| / Reference × 100")
        print("=" * 80 + "\n")

    def analyze(self):
        """Run the complete analysis"""
        if not self.load_data():
            return False

        self.print_statistics()
        return True


def main():
    parser = argparse.ArgumentParser(
        description='Analyze RTD measurement data from CSV files',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s data/rtd_data_20231215_143022.csv
  %(prog)s -f data/rtd_data_20231215_143022.csv
        """
    )

    parser.add_argument('file', nargs='?',
                       help='CSV file to analyze')
    parser.add_argument('--file', '-f', dest='file_arg',
                       help='CSV file to analyze (alternative syntax)')

    args = parser.parse_args()

    # Get the file path from either positional or named argument
    csv_file = args.file or args.file_arg

    if not csv_file:
        parser.print_help()
        sys.exit(1)

    analyzer = RTDAnalyzer(csv_file)
    if analyzer.analyze():
        sys.exit(0)
    else:
        sys.exit(1)


if __name__ == "__main__":
    main()
