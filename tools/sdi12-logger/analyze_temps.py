#!/usr/bin/env python3
"""
Temperature Data Analysis Script
Plots RTD temperatures and M8 Pico temperature, and performs statistical analysis
on RTD sensors 2-7 (which should remain stable).
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

# # Reference resistance values measured with precision ohmmeter
# REFERENCE_RESISTANCES = {
#     'M2_RTD2_temp_C': 104.16,
#     'M3_RTD3_temp_C': 119.87,
#     'M4_RTD4_temp_C': 219.85,
#     'M5_RTD5_temp_C': 269.73,
#     'M6_RTD6_temp_C': 391.85,
#     'M7_RTD7_temp_C': 468.75
# }

# Reference resistance values measured with precision ohmmeter after taking out of breadbaord
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

print(f"\nData summary:")
print(f"  Time range: {df['datetime'].min()} to {df['datetime'].max()}")
print(f"  Duration: {df['datetime'].max() - df['datetime'].min()}")
print(f"  Number of samples: {len(df)}")

# Create figure with separate subplots for each RTD
fig, axes = plt.subplots(4, 2, figsize=(16, 14))
fig.suptitle('Temperature Data Analysis - Overnight Monitoring (Individual RTD Plots)', fontsize=16, fontweight='bold')

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

# Plot RTD 2-7 individually
for idx, col in enumerate(stable_rtd_columns):
    ax = axes[idx + 1]

    # Get reference values
    ref_resistance = REFERENCE_RESISTANCES[col]
    ref_temperature = REFERENCE_TEMPERATURES[col]

    # Plot measured temperature
    ax.plot(df['datetime'], df[col], linewidth=1, color='darkblue')

    # Add horizontal line for expected temperature
    ax.axhline(y=ref_temperature, color='red', linestyle='--', linewidth=1.5,
               label=f'Expected: {ref_temperature:.1f}°C ({ref_resistance:.1f}Ω)')

    # Calculate mean and add as horizontal line
    mean_temp = df[col].mean()
    ax.axhline(y=mean_temp, color='green', linestyle=':', linewidth=1.5,
               label=f'Mean: {mean_temp:.1f}°C')

    ax.set_ylabel('Temperature (°C)', fontsize=10)
    ax.set_title(f'{col.replace("_temp_C", "")} - {ref_resistance:.1f}Ω', fontsize=12, fontweight='bold')
    ax.legend(loc='best', fontsize=8)
    ax.grid(True, alpha=0.3)
    ax.tick_params(axis='x', rotation=45, labelsize=8)

# Set common x-label for bottom plots
axes[6].set_xlabel('Time', fontsize=10)
axes[7].set_xlabel('Time', fontsize=10)

# Hide the last subplot (we only have 7 RTDs + 1 combined plot = 8 plots, last one is empty)
axes[7].axis('off')

plt.tight_layout()
plt.savefig('temperature_plots_individual.png', dpi=300, bbox_inches='tight')
print(f"\nIndividual plots saved as: temperature_plots_individual.png")

# Statistical Analysis for RTD 2-7 (stable sensors)
print("\n" + "="*80)
print("STATISTICAL ANALYSIS FOR STABLE RTD SENSORS (M2-M7)")
print("="*80)

# Calculate resistance from temperature using: R(T) = R0 * (1 + α * T)
print("\nConverting temperatures to resistances using PT100 formula:")
print(f"R(T) = R0 * (1 + α * T), where R0={R0}Ω, α={ALPHA}")
print("\nReference resistances (measured with precision ohmmeter):")
for sensor, ref_r in REFERENCE_RESISTANCES.items():
    ref_temp = REFERENCE_TEMPERATURES[sensor]
    print(f"  {sensor}: {ref_r:.2f} Ω → {ref_temp:.2f} °C (expected)")
print("-"*80)

for col in stable_rtd_columns:
    data = df[col].dropna()

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

    print(f"\n{col}:")
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
print("SUMMARY TABLE - TEMPERATURE MEASUREMENT ERROR ANALYSIS (M2-M7)")
print("="*80)
print(f"{'Sensor':<15} {'Ref Temp (°C)':<15} {'Meas Temp (°C)':<16} {'Error (°C)':<15} {'Std Dev (°C)':<15}")
print("-"*80)

for col in stable_rtd_columns:
    data = df[col].dropna()
    ref_temperature = REFERENCE_TEMPERATURES[col]
    mean_temp = data.mean()
    std_temp = data.std()
    error_temp = mean_temp - ref_temperature

    print(f"{col:<15} {ref_temperature:<15.3f} {mean_temp:<16.3f} {error_temp:<15.3f} {std_temp:<15.3f}")

# Summary statistics table - RESISTANCE ERROR ANALYSIS
print("\n" + "="*80)
print("SUMMARY TABLE - RESISTANCE MEASUREMENT ERROR ANALYSIS (M2-M7)")
print("="*80)
print(f"{'Sensor':<15} {'Ref Res (Ω)':<15} {'Meas Res (Ω)':<15} {'Error (Ω)':<15} {'Error (%)':<12} {'Std Dev (Ω)':<15}")
print("-"*80)

for col in stable_rtd_columns:
    data = df[col].dropna()
    # Convert to resistance
    resistance = R0 * (1 + ALPHA * data)
    ref_resistance = REFERENCE_RESISTANCES[col]
    mean_val = resistance.mean()
    std_val = resistance.std()
    error_val = mean_val - ref_resistance
    error_pct = (error_val / ref_resistance) * 100

    print(f"{col:<15} {ref_resistance:<15.2f} {mean_val:<15.3f} {error_val:<15.3f} {error_pct:<12.3f} {std_val:<15.3f}")

print("\n" + "="*80)
print("ERROR ANALYSIS SUMMARY:")
print("-"*80)

# Calculate overall statistics
all_temp_errors = []
all_temp_std_devs = []
all_resistance_errors = []
all_resistance_std_devs = []
all_resistance_error_pcts = []

for col in stable_rtd_columns:
    data = df[col].dropna()
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
print("DIAGNOSTIC ANALYSIS - ERROR SOURCE IDENTIFICATION")
print("="*80)

# Analyze error pattern to identify root cause
# If error is proportional to resistance -> likely R_ref or excitation current issue
# If error is constant offset -> likely ADC offset error

measured_resistances = []
reference_resistances_list = []
errors_ohms = []

for col in stable_rtd_columns:
    data = df[col].dropna()
    resistance = R0 * (1 + ALPHA * data)
    ref_resistance = REFERENCE_RESISTANCES[col]

    measured_resistances.append(resistance.mean())
    reference_resistances_list.append(ref_resistance)
    errors_ohms.append(resistance.mean() - ref_resistance)

# Convert to numpy arrays for analysis
measured_resistances = np.array(measured_resistances)
reference_resistances_array = np.array(reference_resistances_list)
errors_ohms = np.array(errors_ohms)
percent_errors = (errors_ohms / reference_resistances_array) * 100

# Calculate correlation between reference resistance and error
error_correlation = np.corrcoef(reference_resistances_array, errors_ohms)[0, 1]
percent_error_std = np.std(percent_errors)

print(f"\n1. ERROR PATTERN ANALYSIS:")
print(f"   Correlation between reference resistance and absolute error: {error_correlation:.4f}")
print(f"   Standard deviation of percent errors: {percent_error_std:.4f}%")
print(f"   Mean percent error: {np.mean(percent_errors):.4f}%")

if abs(error_correlation) > 0.9 and percent_error_std < 2.0:
    print(f"\n   ⚠️  PROPORTIONAL ERROR DETECTED (correlation={error_correlation:.3f})")
    print(f"   → Error scales with resistance value")
    print(f"   → Likely causes: R_ref calibration error OR excitation current error")

    # Calculate what R_ref should be to correct the error
    ratio = measured_resistances / reference_resistances_array
    avg_ratio = np.mean(ratio)
    corrected_r_ref = 5030.0 / avg_ratio

    print(f"\n   Measured/Reference ratio: {avg_ratio:.6f}")
    print(f"   If R_ref error: Actual R_ref ≈ {corrected_r_ref:.2f}Ω (nominal: 5030Ω)")
    print(f"   If excitation current error: Actual I_exc ≈ {50.0 / avg_ratio:.2f}µA (nominal: 50µA)")

elif abs(error_correlation) < 0.5:
    print(f"\n   ⚠️  CONSTANT OFFSET ERROR DETECTED (correlation={error_correlation:.3f})")
    print(f"   → Error does not scale with resistance")
    print(f"   → Likely causes: ADC offset error, lead resistance, or ground offset")
    avg_error = np.mean(errors_ohms)
    print(f"\n   Average offset: {avg_error:.3f}Ω")
else:
    print(f"\n   ⚠️  MIXED ERROR PATTERN (correlation={error_correlation:.3f})")
    print(f"   → Both proportional and offset errors present")

print(f"\n2. MEASUREMENT STABILITY ANALYSIS:")
print(f"   Average noise (std dev): {np.mean(all_resistance_std_devs):.3f}Ω")
if np.mean(all_resistance_std_devs) > 1.0:
    print(f"   ⚠️  HIGH NOISE DETECTED")
    print(f"   → Likely causes:")
    print(f"     * ADC settings (filter, gain, speed)")
    print(f"     * Power supply noise")
    print(f"     * Insufficient settling time after mux switching")
    print(f"     * Thermal instability")
elif np.mean(all_resistance_std_devs) > 0.1:
    print(f"   ⚡ MODERATE NOISE")
    print(f"   → Some noise present, but may be acceptable")
else:
    print(f"   ✓ LOW NOISE - Good stability")

print(f"\n3. ERROR vs RESISTANCE ANALYSIS (Non-linearity Check):")
# Check if absolute error or % error decreases with higher resistance
# This would indicate the ADC is better calibrated at higher voltages
abs_percent_errors = np.abs(percent_errors)

# Calculate correlation between resistance and absolute % error
error_vs_resistance_corr = np.corrcoef(reference_resistances_array, abs_percent_errors)[0, 1]
print(f"   Correlation between resistance and absolute % error: {error_vs_resistance_corr:.4f}")

if error_vs_resistance_corr < -0.7:
    print(f"\n   ✓ HIGHER RESISTANCE = LOWER ERROR DETECTED!")
    print(f"   → ADC appears more accurate at higher resistances/voltages")
    print(f"   → This suggests:")
    print(f"     * ADC calibration/tuning optimized for higher voltage range")
    print(f"     * Better SNR at higher signal levels")
    print(f"     * Offset errors dominate at low resistances")
    print(f"\n   RECOMMENDATION:")
    print(f"     * YES - Using higher value resistors would likely improve accuracy")
    print(f"     * Target resistance range: 400-500Ω (like RTD7)")
    print(f"     * Or perform ADC calibration at your actual operating range")
elif error_vs_resistance_corr > 0.7:
    print(f"\n   ⚠️  HIGHER RESISTANCE = HIGHER ERROR")
    print(f"   → ADC appears less accurate at higher resistances")
    print(f"   → Lower resistances might be better")
else:
    print(f"\n   → Error does not strongly correlate with resistance level")
    print(f"   → Non-linearity may not be the dominant issue")

print(f"\n4. DETAILED ERROR BREAKDOWN:")
print(f"   {'Sensor':<15} {'Ref (Ω)':<12} {'Meas (Ω)':<12} {'Error (Ω)':<12} {'% Error':<12} {'Abs % Err':<12}")
print(f"   " + "-"*75)
for i, col in enumerate(stable_rtd_columns):
    print(f"   {col:<15} {reference_resistances_array[i]:<12.2f} {measured_resistances[i]:<12.3f} {errors_ohms[i]:<12.3f} {percent_errors[i]:<12.4f} {abs_percent_errors[i]:<12.4f}")

# Find best and worst performers
best_idx = np.argmin(abs_percent_errors)
worst_idx = np.argmax(abs_percent_errors)
print(f"\n   BEST:  {stable_rtd_columns[best_idx]} at {reference_resistances_array[best_idx]:.2f}Ω (error: {abs_percent_errors[best_idx]:.4f}%)")
print(f"   WORST: {stable_rtd_columns[worst_idx]} at {reference_resistances_array[worst_idx]:.2f}Ω (error: {abs_percent_errors[worst_idx]:.4f}%)")

print("\n" + "="*80)
print("NOTES:")
print("  - Reference temperatures calculated using firmware formula: T = (R/R0 - 1) / α")
print("  - Resistance values calculated using reverse formula: R(T) = R0 * (1 + α * T)")
print("  - Reference resistances measured with precision ohmmeter")
print("  - Error = Measured - Reference (positive = reading high, negative = reading low)")
print("="*80)

plt.show()
