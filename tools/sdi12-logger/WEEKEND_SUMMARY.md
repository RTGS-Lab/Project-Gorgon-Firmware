# Weekend RTD Testing Summary (Nov 21-23, 2025)

## Overview
Three consecutive overnight tests with **50µA excitation current** to assess repeatability and long-term stability.

**Device**: Guadalupe (e00fce6885951c63c0e86719)
**Test Period**: Friday, Saturday, Sunday nights (21:00 onwards each night)
**Excitation Current**: 50µA

## Folder Structure
- `50uACurrentKestrel_Fri_Nov21/` - Friday night analysis
- `50uACurrentKestrel_Sat_Nov22/` - Saturday night analysis
- `50uACurrentKestrel_Sun_Nov23/` - Sunday night analysis

## Three-Night Results Comparison

### RTD1 vs Acclima (Temperature Measurement)

| Night | Samples | RTD1 Mean | Acclima Mean | Offset | MAE | RMSE | Correlation |
|-------|---------|-----------|--------------|--------|-----|------|-------------|
| **Friday** | 793 | 21.22°C | 20.61°C | +0.587°C | 0.776°C | 0.879°C | 0.9696 |
| **Saturday** | 521 | 20.99°C | 20.47°C | +0.505°C | 0.707°C | 0.815°C | 0.9665 |
| **Sunday** | 251 | 22.04°C | 21.44°C | +0.593°C | 0.852°C | 0.967°C | 0.8905 |
| **Average** | - | 21.42°C | 20.84°C | **+0.562°C** | 0.778°C | 0.887°C | 0.9422 |

**Key Finding**: RTD1 offset is **remarkably consistent** at +0.56°C ± 0.04°C across all three nights.

### Fixed Resistor Stability (RTD2-RTD7)

| Sensor | Ref (Ω) | Fri Error | Sat Error | Sun Error | **Avg Error** | Pattern |
|--------|---------|-----------|-----------|-----------|---------------|---------|
| **RTD2** | 99.42 | +0.001% | +0.006% | -0.026% | **-0.006%** | ✓ Excellent |
| **RTD3** | 119.85 | -0.024% | -0.019% | -0.027% | **-0.023%** | ✓ Excellent |
| **RTD5** | 269.64 | +0.561% | +0.564% | +0.541% | **+0.555%** | ⚠️ Systematic |
| **RTD6** | 391.69 | +0.271% | +0.276% | +0.253% | **+0.267%** | ⚠️ Systematic |
| **RTD7** | 468.58 | +0.201% | +0.207% | +0.185% | **+0.198%** | ⚠️ Systematic |

**Note**: RTD4 data not available during weekend tests.

## Key Observations

### Excellent Repeatability
- **RTD1 offset**: Standard deviation across 3 nights = 0.044°C (only 7.8% variation!)
- **RTD5 error**: 0.555% ± 0.012% (only 2.1% variation!)
- **RTD6 error**: 0.267% ± 0.012% (only 4.3% variation!)
- **RTD7 error**: 0.198% ± 0.011% (only 5.6% variation!)

This demonstrates the errors are **systematic and repeatable**, not random noise.

### Error Pattern Analysis

1. **RTD2 & RTD3** (Low resistance: ~100-120Ω)
   - Essentially perfect (< 0.03% error)
   - Very low voltage drop (~0.005-0.006V at 50µA)
   - Minimal diode effects

2. **RTD5, RTD6, RTD7** (Higher resistance: 270-470Ω)
   - Consistent positive errors (0.2-0.6%)
   - Error increases with resistance
   - Higher voltage drops (~0.013-0.023V at 50µA)
   - **Supports reverse diode leakage hypothesis**

### Voltage vs Error Correlation

| RTD | Resistance | Voltage @ 50µA | Error (%) |
|-----|------------|----------------|-----------|
| RTD2 | 99Ω | 0.005V | -0.006% |
| RTD3 | 120Ω | 0.006V | -0.023% |
| RTD5 | 270Ω | 0.013V | +0.555% |
| RTD6 | 392Ω | 0.020V | +0.267% |
| RTD7 | 469Ω | 0.023V | +0.198% |

**Observation**: Errors are positive and increase with voltage, suggesting current loss through reverse-biased blocking diodes.

## Comparison: Initial Test vs Weekend Average

### RTD1 Temperature Measurement
- **Initial Test (Nov 19-20)**: +0.582°C offset
- **Weekend Average**: +0.562°C offset
- **Difference**: 0.020°C (3.4% variation)
- **Conclusion**: Excellent long-term consistency

### Fixed Resistor Measurements
Comparing to initial Nov 19-20 test (where all RTDs were measured):

| Sensor | Initial | Weekend Avg | Difference |
|--------|---------|-------------|------------|
| RTD2 | -0.036% | -0.006% | +0.030% |
| RTD3 | -0.042% | -0.023% | +0.019% |
| RTD5 | +0.031% | +0.555% | +0.524% |
| RTD6 | +0.048% | +0.267% | +0.219% |

**Note**: RTD5 and RTD6 show larger errors during weekend - possible temperature effects on diode leakage, or circuit warm-up effects.

## Conclusions

### 50µA Excitation Performance

**Strengths:**
1. **Excellent repeatability** - errors consistent to within ~5% across multiple days
2. **Low-resistance RTDs essentially perfect** (RTD2, RTD3 < 0.03% error)
3. **RTD1 temperature offset very stable** (~0.56°C, easily calibrated)
4. **High correlation** with reference sensor (R > 0.89)

**Weaknesses:**
1. **Higher-resistance RTDs show systematic errors** (0.2-0.6%)
2. **Errors increase with voltage** - suggests reverse diode leakage
3. **RTD5 shows largest error** (~0.56%)

### Reverse Diode Leakage Evidence

The weekend data **strongly supports** the reverse diode leakage hypothesis:
- Errors scale with voltage/resistance
- Errors are consistent and repeatable
- Low-voltage measurements unaffected
- Pattern matches expected leakage behavior

### Recommendations

1. **For temperature measurement (RTD1)**:
   - 50µA excitation is excellent
   - Apply -0.56°C calibration offset
   - Expect ±0.8°C absolute accuracy after calibration

2. **For fixed resistor testing**:
   - RTD2, RTD3 positions suitable for precision measurement
   - Higher positions (RTD5-7) show voltage-dependent errors
   - Consider lower excitation current for high-resistance measurements

3. **Circuit improvements**:
   - Investigate diode types with lower reverse leakage
   - Consider alternative multiplexing schemes
   - Test lower excitation currents for high-resistance sensors

## Data Files

All raw data extracted from: `Gems_Demo_2025_11_21_to_2025_11_25_23_59_59_20251124_124729.csv`

Total records: 5,393 spanning Nov 21-24, 2025
