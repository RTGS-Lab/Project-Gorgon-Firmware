#!/usr/bin/env python3
"""
Temperature Data Analysis Script - WITH CALIBRATION APPLIED
This script applies the R_ref calibration correction to show what the results
would look like with the corrected firmware value.
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path

# Find the CSV file
csv_file = list(Path('.').glob('sdi12_data_*.csv'))[0]
print(f"Loading data from: {csv_file}")

# Load the data
df = pd.read_csv(csv_file)

# Convert datetime column to datetime type
df['datetime'] = pd.to_datetime(df['datetime'])

# Define the columns for analysis
rtd_columns = [
    'M1_RTD1_temp_C',
    'M2_RTD2_temp_C',
    'M3_RTD3_temp_C',
    'M4_RTD4_temp_C',
    'M5_RTD5_temp_C',
    'M6_RTD6_temp_C',
    'M7_RTD7_temp_C'
]
m8_column = 'M8_Pico_temp_C'

# Columns that should be stable (RTD 2-7)
stable_rtd_columns = rtd_columns[1:]  # M2-M7

# RTD conversion parameters (from firmware: sdi12-analog-mux.c line 229-230)
R0 = 100.0  # PT100 resistance at 0°C (ohms)
ALPHA = 0.00385  # PT100 temperature coefficient

# CALIBRATION VALUES
R_REF_NOMINAL = 5030.0  # Current firmware value
R_REF_CALIBRATED = 5050.5  # Calibrated value from measurements
CALIBRATION_RATIO = R_REF_CALIBRATED / R_REF_NOMINAL

print(f"\nCalibration parameters:")
print(f"  R_ref (nominal):     {R_REF_NOMINAL:.1f}Ω")
print(f"  R_ref (calibrated):  {R_REF_CALIBRATED:.1f}Ω")
print(f"  Calibration ratio:   {CALIBRATION_RATIO:.6f}")
print(f"  Correction:          {((CALIBRATION_RATIO - 1) * 100):.3f}%")

# Reference resistance values measured with precision ohmmeter
REFERENCE_RESISTANCES = {
    'M2_RTD2_temp_C': 99.42,
    'M3_RTD3_temp_C': 119.85,
    'M4_RTD4_temp_C': 219.86,
    'M5_RTD5_temp_C': 269.64,
    'M6_RTD6_temp_C': 391.69,
    'M7_RTD7_temp_C': 468.58
}

# Calculate reference temperatures using firmware formula (ad1724.c line 358)
# T = (R(T)/R0 - 1) / α
REFERENCE_TEMPERATURES = {}
for sensor, ref_r in REFERENCE_RESISTANCES.items():
    ref_temp = (ref_r / R0 - 1.0) / ALPHA
    REFERENCE_TEMPERATURES[sensor] = ref_temp

# Apply calibration correction to temperature data
# Since T = (R/R0 - 1) / α, and R is proportional to R_ref,
# we need to scale the resistance, which means scaling the temperature offset from 0°C
print("\nApplying calibration correction to measured temperatures...")
for col in stable_rtd_columns:
    # Convert temperature to resistance: R(T) = R0 * (1 + α * T)
    resistance_uncalibrated = R0 * (1 + ALPHA * df[col])

    # Apply calibration: scale by ratio
    resistance_calibrated = resistance_uncalibrated * CALIBRATION_RATIO

    # Convert back to temperature: T = (R/R0 - 1) / α
    df[col + '_calibrated'] = (resistance_calibrated / R0 - 1.0) / ALPHA

# Also create calibrated columns for M1 for completeness
df['M1_RTD1_temp_C_calibrated'] = df['M1_RTD1_temp_C']  # Just copy, not analyzing M1

print(f"\nData summary:")
print(f"  Time range: {df['datetime'].min()} to {df['datetime'].max()}")
print(f"  Duration: {df['datetime'].max() - df['datetime'].min()}")
print(f"  Number of samples: {len(df)}")

# Create figure with separate subplots for each RTD
fig, axes = plt.subplots(4, 2, figsize=(16, 14))
fig.suptitle('Temperature Data Analysis - WITH CALIBRATION APPLIED (Individual RTD Plots)', fontsize=16, fontweight='bold')

# Flatten axes array for easier indexing
axes = axes.flatten()

# Plot RTD1 and M8 together (first subplot)
axes[0].plot(df['datetime'], df['M1_RTD1_temp_C'], label='M1 RTD1', color='blue', linewidth=1)
axes[0].plot(df['datetime'], df[m8_column], label='M8 Pico', color='red', linewidth=1)
axes[0].set_ylabel('Temperature (°C)', fontsize=10)
axes[0].set_title('M1 RTD1 and M8 Pico', fontsize=12, fontweight='bold')
axes[0].legend(loc='best', fontsize=9)
axes[0].grid(True, alpha=0.3)
axes[0].tick_params(axis='x', rotation=45, labelsize=8)

# Plot RTD 2-7 individually with calibration applied
for idx, col in enumerate(stable_rtd_columns):
    ax = axes[idx + 1]

    # Get reference values
    ref_resistance = REFERENCE_RESISTANCES[col]
    ref_temperature = REFERENCE_TEMPERATURES[col]

    # Plot calibrated temperature
    cal_col = col + '_calibrated'
    ax.plot(df['datetime'], df[cal_col], linewidth=1, color='darkgreen', label='Calibrated')

    # Add horizontal line for expected temperature
    ax.axhline(y=ref_temperature, color='red', linestyle='--', linewidth=1.5,
               label=f'Expected: {ref_temperature:.1f}°C ({ref_resistance:.1f}Ω)')

    # Calculate mean and add as horizontal line
    mean_temp = df[cal_col].mean()
    ax.axhline(y=mean_temp, color='blue', linestyle=':', linewidth=1.5,
               label=f'Mean: {mean_temp:.1f}°C')

    ax.set_ylabel('Temperature (°C)', fontsize=10)
    ax.set_title(f'{col.replace("_temp_C", "")} - {ref_resistance:.1f}Ω (CALIBRATED)', fontsize=12, fontweight='bold')
    ax.legend(loc='best', fontsize=8)
    ax.grid(True, alpha=0.3)
    ax.tick_params(axis='x', rotation=45, labelsize=8)

# Set common x-label for bottom plots
axes[6].set_xlabel('Time', fontsize=10)
axes[7].set_xlabel('Time', fontsize=10)

# Hide the last subplot (we only have 7 RTDs + 1 combined plot = 8 plots, last one is empty)
axes[7].axis('off')

plt.tight_layout()
plt.savefig('temperature_plots_calibrated.png', dpi=300, bbox_inches='tight')
print(f"\nCalibrated plots saved as: temperature_plots_calibrated.png")

# Statistical Analysis for RTD 2-7 (stable sensors) - WITH CALIBRATION
print("\n" + "="*80)
print("STATISTICAL ANALYSIS FOR STABLE RTD SENSORS (M2-M7) - WITH CALIBRATION")
print("="*80)

print("\nReference resistances (measured with precision ohmmeter):")
for sensor, ref_r in REFERENCE_RESISTANCES.items():
    ref_temp = REFERENCE_TEMPERATURES[sensor]
    print(f"  {sensor}: {ref_r:.2f} Ω → {ref_temp:.2f} °C (expected)")
print("-"*80)

for col in stable_rtd_columns:
    cal_col = col + '_calibrated'
    data = df[cal_col].dropna()

    # Convert temperature to resistance
    resistance = R0 * (1 + ALPHA * data)

    # Get reference values for this sensor
    ref_resistance = REFERENCE_RESISTANCES[col]
    ref_temperature = REFERENCE_TEMPERATURES[col]

    # Calculate resistance errors
    resistance_error = resistance - ref_resistance
    resistance_abs_error = np.abs(resistance_error)
    resistance_percent_error = (resistance_error / ref_resistance) * 100

    # Calculate temperature errors
    temp_error = data - ref_temperature
    temp_abs_error = np.abs(temp_error)

    print(f"\n{col} (CALIBRATED):")
    print(f"  TEMPERATURE (Measured):")
    print(f"    Mean:                {data.mean():.3f} °C")
    print(f"    Std Dev:             {data.std():.3f} °C")
    print(f"    Range:               {data.max() - data.min():.3f} °C")
    print(f"  TEMPERATURE (Reference from {ref_resistance:.2f}Ω):")
    print(f"    Expected:            {ref_temperature:.3f} °C")
    print(f"    Error Mean:          {temp_error.mean():.3f} °C")
    print(f"    Error Std Dev:       {temp_error.std():.3f} °C")
    print(f"    Abs Error Mean:      {temp_abs_error.mean():.3f} °C")
    print(f"    Max Error:           {temp_error.max():.3f} °C")
    print(f"    Min Error:           {temp_error.min():.3f} °C")
    print(f"  ---")
    print(f"  RESISTANCE (Calculated):")
    print(f"    Mean:                {resistance.mean():.3f} Ω")
    print(f"    Median:              {resistance.median():.3f} Ω")
    print(f"    Std Dev:             {resistance.std():.3f} Ω")
    print(f"    Range:               {resistance.max() - resistance.min():.3f} Ω")
    print(f"    CV:                  {(resistance.std() / resistance.mean() * 100):.4f}%")
    print(f"  RESISTANCE (Reference - Ohmmeter):")
    print(f"    Expected:            {ref_resistance:.2f} Ω")
    print(f"    Error Mean:          {resistance_error.mean():.3f} Ω ({resistance_percent_error.mean():.3f}%)")
    print(f"    Error Std Dev:       {resistance_error.std():.3f} Ω")
    print(f"    Abs Error Mean:      {resistance_abs_error.mean():.3f} Ω ({(resistance_abs_error.mean()/ref_resistance*100):.3f}%)")
    print(f"    Max Error:           {resistance_error.max():.3f} Ω ({resistance_percent_error.max():.3f}%)")
    print(f"    Min Error:           {resistance_error.min():.3f} Ω ({resistance_percent_error.min():.3f}%)")

# Summary statistics table - TEMPERATURE ERROR ANALYSIS
print("\n" + "="*80)
print("SUMMARY TABLE - TEMPERATURE MEASUREMENT ERROR (CALIBRATED)")
print("="*80)
print(f"{'Sensor':<15} {'Ref Temp (°C)':<15} {'Meas Temp (°C)':<16} {'Error (°C)':<15} {'Std Dev (°C)':<15}")
print("-"*80)

for col in stable_rtd_columns:
    cal_col = col + '_calibrated'
    data = df[cal_col].dropna()
    ref_temperature = REFERENCE_TEMPERATURES[col]
    mean_temp = data.mean()
    std_temp = data.std()
    error_temp = mean_temp - ref_temperature

    print(f"{col:<15} {ref_temperature:<15.3f} {mean_temp:<16.3f} {error_temp:<15.3f} {std_temp:<15.3f}")

# Summary statistics table - RESISTANCE ERROR ANALYSIS
print("\n" + "="*80)
print("SUMMARY TABLE - RESISTANCE MEASUREMENT ERROR (CALIBRATED)")
print("="*80)
print(f"{'Sensor':<15} {'Ref Res (Ω)':<15} {'Meas Res (Ω)':<15} {'Error (Ω)':<15} {'Error (%)':<12} {'Std Dev (Ω)':<15}")
print("-"*80)

for col in stable_rtd_columns:
    cal_col = col + '_calibrated'
    data = df[cal_col].dropna()
    # Convert to resistance
    resistance = R0 * (1 + ALPHA * data)
    ref_resistance = REFERENCE_RESISTANCES[col]
    mean_val = resistance.mean()
    std_val = resistance.std()
    error_val = mean_val - ref_resistance
    error_pct = (error_val / ref_resistance) * 100

    print(f"{col:<15} {ref_resistance:<15.2f} {mean_val:<15.3f} {error_val:<15.3f} {error_pct:<12.3f} {std_val:<15.3f}")

print("\n" + "="*80)
print("ERROR ANALYSIS SUMMARY (CALIBRATED):")
print("-"*80)

# Calculate overall statistics
all_temp_errors = []
all_temp_std_devs = []
all_resistance_errors = []
all_resistance_std_devs = []
all_resistance_error_pcts = []

for col in stable_rtd_columns:
    cal_col = col + '_calibrated'
    data = df[cal_col].dropna()
    resistance = R0 * (1 + ALPHA * data)
    ref_resistance = REFERENCE_RESISTANCES[col]
    ref_temperature = REFERENCE_TEMPERATURES[col]

    temp_error = data.mean() - ref_temperature
    temp_std = data.std()
    resistance_error = resistance.mean() - ref_resistance
    resistance_error_pct = (resistance_error / ref_resistance) * 100
    resistance_std = resistance.std()

    all_temp_errors.append(abs(temp_error))
    all_temp_std_devs.append(temp_std)
    all_resistance_errors.append(abs(resistance_error))
    all_resistance_std_devs.append(resistance_std)
    all_resistance_error_pcts.append(abs(resistance_error_pct))

print(f"  TEMPERATURE:")
print(f"    Average Absolute Error:    {np.mean(all_temp_errors):.3f} °C")
print(f"    Max Absolute Error:        {np.max(all_temp_errors):.3f} °C")
print(f"    Average Std Deviation:     {np.mean(all_temp_std_devs):.3f} °C")
print(f"    Max Std Deviation:         {np.max(all_temp_std_devs):.3f} °C")
print(f"  RESISTANCE:")
print(f"    Average Absolute Error:    {np.mean(all_resistance_errors):.3f} Ω ({np.mean(all_resistance_error_pcts):.3f}%)")
print(f"    Max Absolute Error:        {np.max(all_resistance_errors):.3f} Ω ({np.max(all_resistance_error_pcts):.3f}%)")
print(f"    Average Std Deviation:     {np.mean(all_resistance_std_devs):.3f} Ω")
print(f"    Max Std Deviation:         {np.max(all_resistance_std_devs):.3f} Ω")

print("\n" + "="*80)
print("NOTES:")
print(f"  - Calibration applied: R_ref changed from {R_REF_NOMINAL}Ω to {R_REF_CALIBRATED}Ω")
print(f"  - Correction factor: {CALIBRATION_RATIO:.6f} ({((CALIBRATION_RATIO-1)*100):.3f}%)")
print(f"  - This shows the expected results after updating firmware")
print("="*80)

plt.show()
