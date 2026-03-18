#include "ad1724.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <math.h>

// SPI Defines
extern spi_inst_t *SPI_PORT;
extern const uint PIN_CS;

// Per-channel software zero-offset calibration
// Each entry stores the raw ADC reading when AINP=AINM for that channel's AIN pair
// These are subtracted from RTD readings to remove per-channel offset error
static uint32_t g_zero_offset[7] = {0, 0, 0, 0, 0, 0, 0};

// Reference voltage correction factor
// This compensates for AVDD-induced variations in excitation current
// Measured by comparing readings with REFIN1 vs internal 2.5V reference
static float g_vref_correction = 1.0f;

void adc_reg_write(uint8_t reg_addr, uint8_t *data, size_t data_len) {
    uint8_t comm_cmd = reg_addr & 0x3F; // Write command (bit 6 = 0)

    gpio_put(PIN_CS, 0);
    spi_write_blocking(SPI_PORT, &comm_cmd, 1);
    if (data != NULL && data_len > 0) {
        spi_write_blocking(SPI_PORT, data, data_len);
    }
    gpio_put(PIN_CS, 1);
}

void adc_reg_read(uint8_t reg_addr, uint8_t *data, size_t data_len) {
    uint8_t comm_cmd = 0x40 | (reg_addr & 0x3F); // Read command (bit 6 = 1)

    gpio_put(PIN_CS, 0);
    spi_write_blocking(SPI_PORT, &comm_cmd, 1);
    if (data != NULL && data_len > 0) {
        spi_read_blocking(SPI_PORT, 0x00, data, data_len);
    }
    gpio_put(PIN_CS, 1);
}

void adc_reset(void) {
    printf("Performing ADC reset (64 consecutive 1s)...\n");

    // Send 64 consecutive 1s with CS low to reset the ADC
    gpio_put(PIN_CS, 0);
    uint8_t reset_data[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // 64 bits of 1s
    spi_write_blocking(SPI_PORT, reset_data, 8);
    gpio_put(PIN_CS, 1);

    // Wait for reset to complete
    sleep_ms(100);

    printf("ADC reset complete\n");
}

bool adc_verify_communication(void) {
    uint8_t id_response = 0;
    adc_reg_read(AD7124_ID_REG, &id_response, 1);
    printf("AD7124 ID register read: 0x%02X (expected: 0x%02X)\n", id_response, AD7124_ID_VALUE);
    return (id_response == AD7124_ID_VALUE);
}

bool adc_configure_rtd(const rtd_config_t *config) {
    printf("Configuring AD7124 for RTD measurement...\n");

    // Step 0: Perform proper ADC reset
    adc_reset();

    // Step 1: Configure excitation current
    printf("Setting excitation current to %d on AIN%d\n", config->excitation_current, config->iout_pin);
    uint8_t ioc1_data[3] = {0};
    uint32_t ioc1_val = (config->excitation_current << 8) | config->iout_pin;
    ioc1_data[0] = (ioc1_val >> 16) & 0xFF;
    ioc1_data[1] = (ioc1_val >> 8) & 0xFF;
    ioc1_data[2] = ioc1_val & 0xFF;
    adc_reg_write(AD7124_IOC_REG1, ioc1_data, 3);
    printf("IO_CONTROL_1 register written: 0x%06X\n", ioc1_val);

    // Step 2: Disable channels 7-15 (we'll configure 0-6 for RTD1-7 later)
    printf("Disabling channels 7-15...\n");
    uint8_t ch_disable[2] = {0x00, 0x01}; // Disabled, but keep default AINP/AINM
    for (int ch = 7; ch <= 15; ch++) {
        adc_reg_write(AD7124_CH0_MAP_REG + ch, ch_disable, 2);
    }

    // Step 3: Configure Channels 0-6 for RTD measurements (RTDs 1-7)
    // Initially, only enable channel 0. We'll dynamically enable channels later.
    // Channel 0: RTD 1 (AIN1/AIN2) - ENABLED
    printf("Configuring Channel 0: RTD 1 on AIN1(+)/AIN2(-) - ENABLED\n");
    uint8_t ch0_data[2] = {0};
    uint16_t ch0_val = AD7124_CH_MAP_REG_CH_ENABLE |
                       AD7124_CH_MAP_REG_SETUP(0) |
                       AD7124_CH_MAP_REG_AINP(AD7124_AIN1) |
                       AD7124_CH_MAP_REG_AINM(AD7124_AIN2);
    ch0_data[0] = (ch0_val >> 8) & 0xFF;
    ch0_data[1] = ch0_val & 0xFF;
    adc_reg_write(AD7124_CH0_MAP_REG, ch0_data, 2);
    printf("Channel 0 register written: 0x%04X\n", ch0_val);

    // Channel 1: RTD 2 (AIN3/AIN4) - DISABLED initially
    printf("Configuring Channel 1: RTD 2 on AIN3(+)/AIN4(-) - DISABLED\n");
    uint8_t ch1_data[2] = {0};
    uint16_t ch1_val = AD7124_CH_MAP_REG_SETUP(0) |
                       AD7124_CH_MAP_REG_AINP(AD7124_AIN3) |
                       AD7124_CH_MAP_REG_AINM(AD7124_AIN4);
    ch1_data[0] = (ch1_val >> 8) & 0xFF;
    ch1_data[1] = ch1_val & 0xFF;
    adc_reg_write(AD7124_CH0_MAP_REG + 1, ch1_data, 2);
    printf("Channel 1 register written: 0x%04X\n", ch1_val);

    // Channel 2: RTD 3 (AIN5/AIN6) - DISABLED initially
    printf("Configuring Channel 2: RTD 3 on AIN5(+)/AIN6(-) - DISABLED\n");
    uint8_t ch2_data[2] = {0};
    uint16_t ch2_val = AD7124_CH_MAP_REG_SETUP(0) |
                       AD7124_CH_MAP_REG_AINP(AD7124_AIN5) |
                       AD7124_CH_MAP_REG_AINM(AD7124_AIN6);
    ch2_data[0] = (ch2_val >> 8) & 0xFF;
    ch2_data[1] = ch2_val & 0xFF;
    adc_reg_write(AD7124_CH0_MAP_REG + 2, ch2_data, 2);
    printf("Channel 2 register written: 0x%04X\n", ch2_val);

    // Channel 3: RTD 4 (AIN7/AIN8) - DISABLED initially
    printf("Configuring Channel 3: RTD 4 on AIN7(+)/AIN8(-) - DISABLED\n");
    uint8_t ch3_data[2] = {0};
    uint16_t ch3_val = AD7124_CH_MAP_REG_SETUP(0) |
                       AD7124_CH_MAP_REG_AINP(AD7124_AIN7) |
                       AD7124_CH_MAP_REG_AINM(AD7124_AIN8);
    ch3_data[0] = (ch3_val >> 8) & 0xFF;
    ch3_data[1] = ch3_val & 0xFF;
    adc_reg_write(AD7124_CH0_MAP_REG + 3, ch3_data, 2);
    printf("Channel 3 register written: 0x%04X\n", ch3_val);

    // Channel 4: RTD 5 (AIN9/AIN10) - DISABLED initially
    // Note: The AD7124-8 has AIN0-AIN15, but not all may be physically accessible
    printf("Configuring Channel 4: RTD 5 on AIN9(+)/AIN10(-) - DISABLED\n");
    uint8_t ch4_data[2] = {0};
    uint16_t ch4_val = AD7124_CH_MAP_REG_SETUP(0) |
                       AD7124_CH_MAP_REG_AINP(0x09) |  // AIN9
                       AD7124_CH_MAP_REG_AINM(0x0A);   // AIN10
    ch4_data[0] = (ch4_val >> 8) & 0xFF;
    ch4_data[1] = ch4_val & 0xFF;
    adc_reg_write(AD7124_CH0_MAP_REG + 4, ch4_data, 2);
    printf("Channel 4 register written: 0x%04X\n", ch4_val);

    // Channel 5: RTD 6 (AIN11/AIN12) - DISABLED initially
    printf("Configuring Channel 5: RTD 6 on AIN11(+)/AIN12(-) - DISABLED\n");
    uint8_t ch5_data[2] = {0};
    uint16_t ch5_val = AD7124_CH_MAP_REG_SETUP(0) |
                       AD7124_CH_MAP_REG_AINP(0x0B) |  // AIN11
                       AD7124_CH_MAP_REG_AINM(0x0C);   // AIN12
    ch5_data[0] = (ch5_val >> 8) & 0xFF;
    ch5_data[1] = ch5_val & 0xFF;
    adc_reg_write(AD7124_CH0_MAP_REG + 5, ch5_data, 2);
    printf("Channel 5 register written: 0x%04X\n", ch5_val);

    // Channel 6: RTD 7 (AIN13/AIN14) - DISABLED initially
    printf("Configuring Channel 6: RTD 7 on AIN13(+)/AIN14(-) - DISABLED\n");
    uint8_t ch6_data[2] = {0};
    uint16_t ch6_val = AD7124_CH_MAP_REG_SETUP(0) |
                       AD7124_CH_MAP_REG_AINP(0x0D) |  // AIN13
                       AD7124_CH_MAP_REG_AINM(0x0E);   // AIN14
    ch6_data[0] = (ch6_val >> 8) & 0xFF;
    ch6_data[1] = ch6_val & 0xFF;
    adc_reg_write(AD7124_CH0_MAP_REG + 6, ch6_data, 2);
    printf("Channel 6 register written: 0x%04X\n", ch6_val);

    // Step 4: Configure Setup 0 for ratiometric measurement using REFIN1
    printf("Configuring Setup 0: REF=REFIN1, PGA=1, Buffers enabled (ratiometric)\n");
    uint8_t cfg0_data[2] = {0};
    uint16_t cfg0_val = AD7124_CFG_REG_REF_BUFP |      // Enable ref buffer +
                        AD7124_CFG_REG_REF_BUFM |      // Enable ref buffer -
                        AD7124_CFG_REG_AIN_BUFP |      // Enable analog input buffer +
                        AD7124_CFG_REG_AIN_BUFM |      // Enable analog input buffer -
                        AD7124_CFG_REG_REF_SEL(AD7124_REFIN1) | // Use REFIN1 as reference
                        AD7124_CFG_REG_PGA(AD7124_PGA_1);       // Gain = 1
    cfg0_data[0] = (cfg0_val >> 8) & 0xFF;
    cfg0_data[1] = cfg0_val & 0xFF;
    adc_reg_write(AD7124_CFG0_REG, cfg0_data, 2);
    printf("Config 0 register written: 0x%04X\n", cfg0_val);

    // Step 4: Configure Filter 0 for better stability and noise rejection
    printf("Configuring Filter 0: SINC4, 50/60Hz rejection, FS=192 (slower, more stable)\n");
    uint8_t filt0_data[3] = {0};
    uint32_t filt0_val = AD7124_FILT_REG_FILTER(AD7124_SINC4_FILTER) |
                         AD7124_FILT_REG_REJ60 |  // Enable 50/60Hz rejection
                         AD7124_FILT_REG_FS(192); // Slower for better stability
    filt0_data[0] = (filt0_val >> 16) & 0xFF;
    filt0_data[1] = (filt0_val >> 8) & 0xFF;
    filt0_data[2] = filt0_val & 0xFF;
    adc_reg_write(AD7124_FILTER0_REG, filt0_data, 3);
    printf("Filter 0 register written: 0x%06X\n", filt0_val);

    // Step 5: Set to standby mode (we'll use single conversion mode, triggered manually)
    printf("Setting ADC Control: External ref (REFIN1), standby mode, low power\n");
    uint8_t adc_ctrl_data[2] = {0};
    uint16_t adc_ctrl_val = AD7124_ADC_CTRL_DATA_STATUS |               // Enable status with data
                            AD7124_ADC_CTRL_POWER_MODE(0) |             // Low power mode (0=low, 1=mid, 2=full)
                            AD7124_ADC_CTRL_MODE(AD7124_MODE_STANDBY) | // Standby mode (we'll trigger single conversions)
                            AD7124_ADC_CTRL_CLK_SEL(0);                 // Internal clock
    // Note: REF_EN=0 because we're using external reference on REFIN1
    adc_ctrl_data[0] = (adc_ctrl_val >> 8) & 0xFF;
    adc_ctrl_data[1] = adc_ctrl_val & 0xFF;
    adc_reg_write(AD7124_ADC_CTRL_REG, adc_ctrl_data, 2);
    printf("ADC Control register written: 0x%04X\n", adc_ctrl_val);

    // Step 6: Wait for excitation current and REFIN1 to stabilize
    // The excitation current needs time to settle through the RTD circuit
    // and establish a stable reference voltage across R_ref
    printf("\n=== Waiting for REFIN1 reference to stabilize (500ms) ===\n");
    sleep_ms(500);

    // Step 7: Per-Channel Software Zero-Offset Calibration
    // Instead of using the ADC's internal calibration (which doesn't work for
    // ratiometric measurements), we measure "zero" ourselves by configuring
    // each channel with AINP=AINM (same pin = shorted inputs).
    // Each channel uses different AIN pins, so each may have different offset.
    printf("\n=== Per-Channel Software Zero-Offset Calibration ===\n");

    // AIN pins for each RTD channel (positive input pin - we'll use same for both AINP and AINM)
    const uint8_t ain_pins[7] = {
        AD7124_AIN1,  // RTD1: normally AIN1/AIN2, measure AIN1/AIN1
        AD7124_AIN3,  // RTD2: normally AIN3/AIN4, measure AIN3/AIN3
        AD7124_AIN5,  // RTD3: normally AIN5/AIN6, measure AIN5/AIN5
        AD7124_AIN7,  // RTD4: normally AIN7/AIN8, measure AIN7/AIN7
        0x09,         // RTD5: normally AIN9/AIN10, measure AIN9/AIN9
        0x0B,         // RTD6: normally AIN11/AIN12, measure AIN11/AIN11
        0x0D          // RTD7: normally AIN13/AIN14, measure AIN13/AIN13
    };

    // Measure zero offset for each channel
    for (int ch = 0; ch < 7; ch++) {
        // Configure channel 0 with AINP=AINM for this RTD's AIN pin
        uint8_t ch_zero[2];
        uint16_t ch_zero_val = AD7124_CH_MAP_REG_CH_ENABLE |
                               AD7124_CH_MAP_REG_SETUP(0) |
                               AD7124_CH_MAP_REG_AINP(ain_pins[ch]) |
                               AD7124_CH_MAP_REG_AINM(ain_pins[ch]);  // Same pin!
        ch_zero[0] = (ch_zero_val >> 8) & 0xFF;
        ch_zero[1] = ch_zero_val & 0xFF;
        adc_reg_write(AD7124_CH0_MAP_REG, ch_zero, 2);

        // Wait for settling
        sleep_ms(20);

        // Start a single conversion
        adc_ctrl_val = AD7124_ADC_CTRL_DATA_STATUS |
                       AD7124_ADC_CTRL_POWER_MODE(0) |
                       AD7124_ADC_CTRL_MODE(AD7124_MODE_SINGLE) |
                       AD7124_ADC_CTRL_CLK_SEL(0);
        adc_ctrl_data[0] = (adc_ctrl_val >> 8) & 0xFF;
        adc_ctrl_data[1] = adc_ctrl_val & 0xFF;
        adc_reg_write(AD7124_ADC_CTRL_REG, adc_ctrl_data, 2);

        // Wait for conversion to complete
        uint8_t status = 0x80;
        int timeout = 2000;
        while ((status & 0x80) && timeout > 0) {
            sleep_ms(1);
            adc_reg_read(AD7124_STATUS_REG, &status, 1);
            timeout--;
        }

        if (timeout <= 0) {
            printf("Ch%d: TIMEOUT\n", ch);
            g_zero_offset[ch] = 0;
        } else {
            // Read the data (24-bit + status byte)
            uint8_t data_bytes[4];
            adc_reg_read(AD7124_DATA_REG, data_bytes, 4);
            g_zero_offset[ch] = ((uint32_t)data_bytes[0] << 16) |
                                ((uint32_t)data_bytes[1] << 8) |
                                data_bytes[2];

            // Convert to equivalent resistance for display
            float zero_ratio = (float)g_zero_offset[ch] / 16777216.0f;
            float zero_ohms = config->r_ref * zero_ratio;

            printf("Ch%d: 0x%06X (%6u) = %.3f ohms (AIN%d/AIN%d)\n",
                   ch, g_zero_offset[ch], g_zero_offset[ch], zero_ohms,
                   ain_pins[ch], ain_pins[ch]);
        }
    }

    // Restore channel 0 to normal RTD1 configuration (AIN1/AIN2)
    uint8_t ch0_restore[2];
    uint16_t ch0_restore_val = AD7124_CH_MAP_REG_CH_ENABLE |
                               AD7124_CH_MAP_REG_SETUP(0) |
                               AD7124_CH_MAP_REG_AINP(AD7124_AIN1) |
                               AD7124_CH_MAP_REG_AINM(AD7124_AIN2);
    ch0_restore[0] = (ch0_restore_val >> 8) & 0xFF;
    ch0_restore[1] = ch0_restore_val & 0xFF;
    adc_reg_write(AD7124_CH0_MAP_REG, ch0_restore, 2);

    // Return to standby mode
    adc_ctrl_val = AD7124_ADC_CTRL_DATA_STATUS |
                   AD7124_ADC_CTRL_POWER_MODE(0) |
                   AD7124_ADC_CTRL_MODE(AD7124_MODE_STANDBY) |
                   AD7124_ADC_CTRL_CLK_SEL(0);
    adc_ctrl_data[0] = (adc_ctrl_val >> 8) & 0xFF;
    adc_ctrl_data[1] = adc_ctrl_val & 0xFF;
    adc_reg_write(AD7124_ADC_CTRL_REG, adc_ctrl_data, 2);

    printf("=== Per-Channel Zero-Offset Calibration Complete ===\n\n");

    // Step 8: Measure AVDD using Pico's internal temperature sensor
    // The temp sensor outputs ~0.706V (independent of AVDD), but the ADC reading
    // depends on AVDD since it's the ADC reference. This lets us calculate AVDD.
    //
    // V_temp ≈ 0.706V at 27°C
    // ADC_reading = V_temp / AVDD × 4096
    // AVDD = V_temp × 4096 / ADC_reading
    printf("=== Measuring AVDD via Pico ADC ===\n");

    // Read Pico's internal temperature sensor ADC
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);  // Channel 4 = internal temp sensor

    // Average multiple readings for stability
    uint32_t adc_sum = 0;
    for (int i = 0; i < 16; i++) {
        adc_sum += adc_read();
        sleep_ms(1);
    }
    uint16_t adc_avg = adc_sum / 16;

    // Calculate AVDD
    // V_temp at 27°C ≈ 0.706V (from RP2040 datasheet)
    // We use a slightly adjusted value based on typical readings
    const float v_temp_nominal = 0.706f;
    float avdd_measured = v_temp_nominal * 4096.0f / (float)adc_avg;

    printf("Pico temp sensor ADC: %u (avg of 16)\n", adc_avg);
    printf("Calculated AVDD: %.4f V\n", avdd_measured);
    printf("Nominal AVDD:    3.3000 V\n");

    // Calculate correction factor
    // If AVDD is higher, excitation current is higher, V_REFIN1 is higher,
    // and ratiometric readings are unaffected... BUT the excitation current
    // source in the AD7124 is referenced to AVDD, so higher AVDD = higher I_exc.
    // However, since both V_RTD and V_REF scale with I_exc, this should cancel.
    //
    // The issue is likely that leakage current through the protection diodes
    // scales with AVDD (higher AVDD = more leakage). This adds to I_exc on
    // the R_ref side but not proportionally on the RTD side.
    //
    // Empirically: higher AVDD → more diode leakage → higher V_REFIN1 → LOWER readings
    // So we scale UP when AVDD is high: correction = AVDD / 3.3
    const float nominal_avdd = 3.3f;
    g_vref_correction = avdd_measured / nominal_avdd;

    printf("AVDD correction factor: %.6f\n", g_vref_correction);
    printf("=== AVDD Measurement Complete ===\n\n");

    // Verify configuration by reading back registers
    printf("\n=== Verifying Configuration ===\n");

    // Read back IO_CONTROL_1
    uint8_t ioc1_readback[3];
    adc_reg_read(AD7124_IOC_REG1, ioc1_readback, 3);
    uint32_t ioc1_read = ((uint32_t)ioc1_readback[0] << 16) |
                         ((uint32_t)ioc1_readback[1] << 8) |
                         ioc1_readback[2];
    printf("IO_CONTROL_1 readback: 0x%06X\n", ioc1_read);

    // Read back Channel 0
    uint8_t ch0_readback[2];
    adc_reg_read(AD7124_CH0_MAP_REG, ch0_readback, 2);
    uint16_t ch0_read = ((uint16_t)ch0_readback[0] << 8) | ch0_readback[1];
    printf("Channel 0 readback: 0x%04X\n", ch0_read);

    // Read back Channel 1 to verify it's disabled
    uint8_t ch1_readback[2];
    adc_reg_read(AD7124_CH1_MAP_REG, ch1_readback, 2);
    uint16_t ch1_read = ((uint16_t)ch1_readback[0] << 8) | ch1_readback[1];
    printf("Channel 1 readback: 0x%04X (should be disabled)\n", ch1_read);

    // Read back Config 0
    uint8_t cfg0_readback[2];
    adc_reg_read(AD7124_CFG0_REG, cfg0_readback, 2);
    uint16_t cfg0_read = ((uint16_t)cfg0_readback[0] << 8) | cfg0_readback[1];
    printf("Config 0 readback: 0x%04X\n", cfg0_read);

    // Read back Filter 0
    uint8_t filt0_readback[3];
    adc_reg_read(AD7124_FILTER0_REG, filt0_readback, 3);
    uint32_t filt0_read = ((uint32_t)filt0_readback[0] << 16) |
                          ((uint32_t)filt0_readback[1] << 8) |
                          filt0_readback[2];
    printf("Filter 0 readback: 0x%06X\n", filt0_read);

    // Read back ADC Control
    uint8_t ctrl_readback[2];
    adc_reg_read(AD7124_ADC_CTRL_REG, ctrl_readback, 2);
    uint16_t ctrl_read = ((uint16_t)ctrl_readback[0] << 8) | ctrl_readback[1];
    printf("ADC Control readback: 0x%04X\n", ctrl_read);

    // Check error register
    uint8_t error_readback[3];
    adc_reg_read(AD7124_ERROR_REG, error_readback, 3);
    uint32_t error_read = ((uint32_t)error_readback[0] << 16) |
                          ((uint32_t)error_readback[1] << 8) |
                          error_readback[2];
    printf("Error register: 0x%06X\n", error_read);

    printf("================================\n\n");

    printf("RTD configuration complete!\n");

    return true;
}

void adc_enable_single_channel(uint8_t channel) {
    // Enable only the specified channel (0-6), disable all others
    printf("Enabling only ADC channel %d...\n", channel);

    // Define channel configurations (AINP/AINM pairs for each RTD)
    const uint8_t ain_pairs[7][2] = {
        {AD7124_AIN1, AD7124_AIN2},   // Channel 0: RTD1
        {AD7124_AIN3, AD7124_AIN4},   // Channel 1: RTD2
        {AD7124_AIN5, AD7124_AIN6},   // Channel 2: RTD3
        {AD7124_AIN7, AD7124_AIN8},   // Channel 3: RTD4
        {0x09, 0x0A},                 // Channel 4: RTD5 (AIN9/AIN10)
        {0x0B, 0x0C},                 // Channel 5: RTD6 (AIN11/AIN12)
        {0x0D, 0x0E}                  // Channel 6: RTD7 (AIN13/AIN14)
    };

    // Configure all 7 channels
    for (uint8_t ch = 0; ch < 7; ch++) {
        uint8_t ch_data[2];
        uint16_t ch_val;

        if (ch == channel) {
            // Enable this channel
            ch_val = AD7124_CH_MAP_REG_CH_ENABLE |
                     AD7124_CH_MAP_REG_SETUP(0) |
                     AD7124_CH_MAP_REG_AINP(ain_pairs[ch][0]) |
                     AD7124_CH_MAP_REG_AINM(ain_pairs[ch][1]);
        } else {
            // Disable this channel
            ch_val = AD7124_CH_MAP_REG_SETUP(0) |
                     AD7124_CH_MAP_REG_AINP(ain_pairs[ch][0]) |
                     AD7124_CH_MAP_REG_AINM(ain_pairs[ch][1]);
        }

        ch_data[0] = (ch_val >> 8) & 0xFF;
        ch_data[1] = ch_val & 0xFF;
        adc_reg_write(AD7124_CH0_MAP_REG + ch, ch_data, 2);
    }
}

void adc_start_single_conversion(void) {
    // Start a single conversion by writing to ADC_CONTROL register
    uint8_t adc_ctrl_data[2];
    uint16_t adc_ctrl_val = AD7124_ADC_CTRL_DATA_STATUS |               // Enable status with data
                            AD7124_ADC_CTRL_POWER_MODE(0) |             // Low power mode
                            AD7124_ADC_CTRL_MODE(AD7124_MODE_SINGLE) |  // Single conversion mode
                            AD7124_ADC_CTRL_CLK_SEL(0);                 // Internal clock
    adc_ctrl_data[0] = (adc_ctrl_val >> 8) & 0xFF;
    adc_ctrl_data[1] = adc_ctrl_val & 0xFF;
    adc_reg_write(AD7124_ADC_CTRL_REG, adc_ctrl_data, 2);
}

bool adc_read_rtd_data(uint32_t *rtd_data, uint8_t *channel) {
    // Check if data is ready
    uint8_t status;
    adc_reg_read(AD7124_STATUS_REG, &status, 1);

    printf("Status register: 0x%02X (RDY bit: %s)\n", status, (status & 0x80) ? "NOT READY" : "READY");

    if (status & 0x80) { // RDY bit is high, no data ready
        return false;
    }

    printf("Data ready! Reading data register...\n");

    // Since DATA_STATUS is enabled, read 4 bytes (24-bit data + 8-bit status)
    uint8_t data_bytes[4] = {0};
    adc_reg_read(AD7124_DATA_REG, data_bytes, 4);

    // Extract 24-bit ADC data (first 3 bytes)
    *rtd_data = ((uint32_t)data_bytes[0] << 16) |
                ((uint32_t)data_bytes[1] << 8) |
                data_bytes[2];

    // Get channel from appended status byte (4th byte)
    *channel = data_bytes[3] & 0x0F;

    printf("Raw data bytes: 0x%02X 0x%02X 0x%02X 0x%02X\n", data_bytes[0], data_bytes[1], data_bytes[2], data_bytes[3]);
    printf("RTD Data: 0x%06X (%d), Channel: %d (from data read)\n", *rtd_data, *rtd_data, *channel);

    return true;
}

float adc_calculate_resistance(uint32_t rtd_data, const rtd_config_t *config, uint8_t channel) {
    // Apply per-channel software zero-offset correction
    // This removes the ADC offset error measured during calibration for this specific channel
    uint32_t offset = (channel < 7) ? g_zero_offset[channel] : 0;
    int32_t corrected_data = (int32_t)rtd_data - (int32_t)offset;

    // Clamp to non-negative (shouldn't happen with valid readings)
    if (corrected_data < 0) {
        corrected_data = 0;
    }

    // Convert 24-bit unsigned to voltage ratio (ratiometric measurement)
    float ratio = (float)corrected_data / 16777216.0f; // 2^24

    // Calculate RTD resistance: R_rtd = R_ref * ratio
    float r_rtd = config->r_ref * ratio;

    // Apply reference voltage correction factor
    // This compensates for AVDD-induced excitation current variations
    r_rtd *= g_vref_correction;

    printf("RTD%d: %.2f ohms (raw: %u, offset: %u, vref_corr: %.4f)\n",
           channel + 1, r_rtd, rtd_data, offset, g_vref_correction);

    return r_rtd;
}

float adc_calculate_temperature(uint32_t rtd_data, const rtd_config_t *config, uint8_t channel) {
    // Calculate resistance first
    float r_rtd = adc_calculate_resistance(rtd_data, config, channel);

    // Calculate temperature using simplified RTD equation: R(T) = R0(1 + α*T)
    // Therefore: T = (R(T)/R0 - 1) / α
    float temperature = (r_rtd / config->r_rtd_0 - 1.0f) / config->alpha;

    printf("Temperature calculated: %.2f°C\n", temperature);

    return temperature;
}

float adc_read_internal_temperature(void) {
    printf("Reading AD7124 internal temperature sensor...\n");

    // Step 1: Configure channel 3 for internal temperature measurement
    // AINP = Temperature sensor (0x10), AINM = AVSS (0x11)
    printf("Configuring Channel 3 for temperature sensor\n");
    uint8_t ch3_data[2];
    uint16_t ch3_val = AD7124_CH_MAP_REG_CH_ENABLE |
                       AD7124_CH_MAP_REG_SETUP(1) |  // Use Setup 1 for temperature
                       AD7124_CH_MAP_REG_AINP(AD7124_TEMP) |
                       AD7124_CH_MAP_REG_AINM(AD7124_AVSS);
    ch3_data[0] = (ch3_val >> 8) & 0xFF;
    ch3_data[1] = ch3_val & 0xFF;
    adc_reg_write(AD7124_CH0_MAP_REG + 3, ch3_data, 2);

    // Disable other channels (0-2 for RTD1-3, 4-6 for RTD4-7, 7-15 unused)
    uint8_t ch_disable[2] = {0x00, 0x01};
    for (int ch = 0; ch < 3; ch++) {
        adc_reg_write(AD7124_CH0_MAP_REG + ch, ch_disable, 2);
    }
    for (int ch = 4; ch <= 6; ch++) {
        adc_reg_write(AD7124_CH0_MAP_REG + ch, ch_disable, 2);
    }
    for (int ch = 7; ch <= 15; ch++) {
        adc_reg_write(AD7124_CH0_MAP_REG + ch, ch_disable, 2);
    }

    // Step 2: Configure Setup 1 for BIPOLAR measurement with internal reference
    // The datasheet formula assumes bipolar mode!
    printf("Configuring Setup 1 for temperature measurement (BIPOLAR mode)\n");
    uint8_t cfg1_data[2];
    uint16_t cfg1_val = AD7124_CFG_REG_BIPOLAR |       // BIPOLAR mode (important!)
                        AD7124_CFG_REG_REF_BUFP |      // Enable ref buffer +
                        AD7124_CFG_REG_REF_BUFM |      // Enable ref buffer -
                        AD7124_CFG_REG_AIN_BUFP |      // Enable analog input buffer +
                        AD7124_CFG_REG_AIN_BUFM |      // Enable analog input buffer -
                        AD7124_CFG_REG_REF_SEL(AD7124_INT_REF) |  // Use internal 2.5V reference
                        AD7124_CFG_REG_PGA(AD7124_PGA_1);         // Gain = 1
    cfg1_data[0] = (cfg1_val >> 8) & 0xFF;
    cfg1_data[1] = cfg1_val & 0xFF;
    adc_reg_write(AD7124_CFG1_REG, cfg1_data, 2);

    // Step 3: Configure Filter 1
    printf("Configuring Filter 1\n");
    uint8_t filt1_data[3];
    uint32_t filt1_val = AD7124_FILT_REG_FILTER(AD7124_SINC4_FILTER) |
                         AD7124_FILT_REG_FS(192);
    filt1_data[0] = (filt1_val >> 16) & 0xFF;
    filt1_data[1] = (filt1_val >> 8) & 0xFF;
    filt1_data[2] = filt1_val & 0xFF;
    adc_reg_write(AD7124_FILTER1_REG, filt1_data, 3);

    // Step 4: Enable internal reference
    printf("Enabling internal reference\n");
    uint8_t adc_ctrl_data[2];
    uint16_t adc_ctrl_val = AD7124_ADC_CTRL_DATA_STATUS |
                            AD7124_ADC_CTRL_REF_EN |                // Enable internal reference
                            AD7124_ADC_CTRL_POWER_MODE(0) |
                            AD7124_ADC_CTRL_MODE(AD7124_MODE_STANDBY) |
                            AD7124_ADC_CTRL_CLK_SEL(0);
    adc_ctrl_data[0] = (adc_ctrl_val >> 8) & 0xFF;
    adc_ctrl_data[1] = adc_ctrl_val & 0xFF;
    adc_reg_write(AD7124_ADC_CTRL_REG, adc_ctrl_data, 2);

    // Wait for reference to settle
    sleep_ms(50);

    // Step 5: Start conversion
    adc_start_single_conversion();

    // Step 6: Wait for conversion to complete
    uint8_t status;
    int timeout = 100;
    bool conversion_ready = false;

    while (timeout > 0) {
        adc_reg_read(AD7124_STATUS_REG, &status, 1);
        if ((status & 0x80) == 0) {  // RDY bit is low = data ready
            conversion_ready = true;
            break;
        }
        sleep_ms(10);
        timeout--;
    }

    if (!conversion_ready) {
        printf("ERROR: Temperature conversion timeout\n");
        return -999.0f;
    }

    // Step 7: Read the data
    uint32_t temp_data;
    uint8_t channel;
    if (adc_read_rtd_data(&temp_data, &channel)) {
        // In bipolar mode, the data is in two's complement format
        // Convert 24-bit two's complement to signed 32-bit integer
        int32_t signed_data;
        if (temp_data & 0x800000) {
            // Negative number - sign extend
            signed_data = (int32_t)(temp_data | 0xFF000000);
        } else {
            // Positive number
            signed_data = (int32_t)temp_data;
        }

        // Calculate temperature using AD7124 formula (for bipolar mode):
        // Temperature (°C) = (Conversion / 13584) - 272.5
        // The datasheet says: sensitivity is 13,584 codes/°C
        float temperature = ((float)signed_data / 13584.0f) - 272.5f;

        printf("AD7124 Internal Temperature: %.2f°C (Raw: 0x%06X, Signed: %d)\n",
               temperature, temp_data, signed_data);
        return temperature;
    } else {
        printf("ERROR: Failed to read temperature data\n");
        return -999.0f;
    }
}

float pico_read_internal_temperature(void) {
    printf("Reading Raspberry Pi Pico internal temperature...\n");

    // Initialize ADC (safe to call multiple times)
    adc_init();

    // Enable temperature sensor
    adc_set_temp_sensor_enabled(true);

    // Select ADC input 4 (temperature sensor)
    adc_select_input(4);

    // Read ADC value (12-bit)
    uint16_t adc_value = adc_read();

    // Convert to voltage (assuming 3.3V reference)
    float voltage = adc_value * 3.3f / 4096.0f;

    // Convert to temperature using formula from datasheet:
    // T = 27 - (ADC_voltage - 0.706)/0.001721
    float temperature = 27.0f - (voltage - 0.706f) / 0.001721f;

    printf("Pico Internal Temperature: %.2f°C (ADC: %d, Voltage: %.3fV)\n",
           temperature, adc_value, voltage);

    return temperature;
}

float adc_measure_vref(float expected_vref) {
    // Measure V_REFIN1 using the internal 2.5V reference.
    // We measure V_RTD with the 2.5V reference to get absolute voltage,
    // then compare to the ratiometric reading to derive V_REFIN1.
    //
    // This detects diode leakage current that adds to the excitation current,
    // causing V_REFIN1 to be higher than expected: V = (I_exc + I_leak) * R_ref

    printf("\n=== Measuring V_REFIN1 for Leakage Correction ===\n");

    uint8_t ch_data[2];
    uint8_t cfg_data[2];
    uint8_t ctrl_data[2];
    uint8_t status;
    uint8_t data_bytes[4];
    int timeout;

    // Disable all channels first
    uint8_t ch_disable[2] = {0x00, 0x01};
    for (int ch = 0; ch <= 15; ch++) {
        adc_reg_write(AD7124_CH0_MAP_REG + ch, ch_disable, 2);
    }

    // Configure channel 0 for RTD1 (AIN0/AIN1) using Setup 1 with internal reference
    uint16_t ch_val = AD7124_CH_MAP_REG_CH_ENABLE |
                      AD7124_CH_MAP_REG_SETUP(1) |  // Use Setup 1 for this measurement
                      AD7124_CH_MAP_REG_AINP(AD7124_AIN1) |
                      AD7124_CH_MAP_REG_AINM(AD7124_AIN2);
    ch_data[0] = (ch_val >> 8) & 0xFF;
    ch_data[1] = ch_val & 0xFF;
    adc_reg_write(AD7124_CH0_MAP_REG, ch_data, 2);

    // Configure Setup 1 to use internal 2.5V reference
    uint16_t cfg_val = AD7124_CFG_REG_REF_BUFP |
                       AD7124_CFG_REG_REF_BUFM |
                       AD7124_CFG_REG_AIN_BUFP |
                       AD7124_CFG_REG_AIN_BUFM |
                       AD7124_CFG_REG_REF_SEL(AD7124_INT_REF) |
                       AD7124_CFG_REG_PGA(AD7124_PGA_1);
    cfg_data[0] = (cfg_val >> 8) & 0xFF;
    cfg_data[1] = cfg_val & 0xFF;
    adc_reg_write(AD7124_CFG1_REG, cfg_data, 2);  // Write to CFG1, not CFG0

    // Configure Filter 1 (same as Filter 0)
    uint8_t filt_data[3];
    uint32_t filt_val = AD7124_FILT_REG_FILTER(AD7124_SINC4_FILTER) |
                        AD7124_FILT_REG_REJ60 |
                        AD7124_FILT_REG_FS(192);
    filt_data[0] = (filt_val >> 16) & 0xFF;
    filt_data[1] = (filt_val >> 8) & 0xFF;
    filt_data[2] = filt_val & 0xFF;
    adc_reg_write(AD7124_FILTER1_REG, filt_data, 3);

    // Enable internal reference
    uint16_t ctrl_val = AD7124_ADC_CTRL_DATA_STATUS |
                        AD7124_ADC_CTRL_REF_EN |
                        AD7124_ADC_CTRL_POWER_MODE(0) |
                        AD7124_ADC_CTRL_MODE(AD7124_MODE_STANDBY) |
                        AD7124_ADC_CTRL_CLK_SEL(0);
    ctrl_data[0] = (ctrl_val >> 8) & 0xFF;
    ctrl_data[1] = ctrl_val & 0xFF;
    adc_reg_write(AD7124_ADC_CTRL_REG, ctrl_data, 2);

    sleep_ms(50);  // Let internal reference settle

    // Start conversion
    ctrl_val = AD7124_ADC_CTRL_DATA_STATUS |
               AD7124_ADC_CTRL_REF_EN |
               AD7124_ADC_CTRL_POWER_MODE(0) |
               AD7124_ADC_CTRL_MODE(AD7124_MODE_SINGLE) |
               AD7124_ADC_CTRL_CLK_SEL(0);
    ctrl_data[0] = (ctrl_val >> 8) & 0xFF;
    ctrl_data[1] = ctrl_val & 0xFF;
    adc_reg_write(AD7124_ADC_CTRL_REG, ctrl_data, 2);

    // Wait for conversion
    status = 0x80;
    timeout = 2000;
    while ((status & 0x80) && timeout > 0) {
        sleep_ms(1);
        adc_reg_read(AD7124_STATUS_REG, &status, 1);
        timeout--;
    }

    if (timeout <= 0) {
        printf("V_RTD measurement timeout!\n");
        g_vref_correction = 1.0f;
        return 0.0f;
    }

    adc_reg_read(AD7124_DATA_REG, data_bytes, 4);
    uint32_t adc_int = ((uint32_t)data_bytes[0] << 16) |
                       ((uint32_t)data_bytes[1] << 8) |
                       data_bytes[2];

    // Calculate V_RTD from internal reference measurement
    // V_RTD = 2.5V × (ADC / 2^24)
    float v_rtd = 2.5f * ((float)adc_int / 16777216.0f);

    printf("RTD1 ADC (2.5V ref): 0x%06X (%u)\n", adc_int, adc_int);
    printf("V_RTD: %.6f V\n", v_rtd);

    // Now measure with REFIN1 to get the ratio
    ch_val = AD7124_CH_MAP_REG_CH_ENABLE |
             AD7124_CH_MAP_REG_SETUP(0) |  // Use Setup 0 (REFIN1)
             AD7124_CH_MAP_REG_AINP(AD7124_AIN0) |
             AD7124_CH_MAP_REG_AINM(AD7124_AIN1);
    ch_data[0] = (ch_val >> 8) & 0xFF;
    ch_data[1] = ch_val & 0xFF;
    adc_reg_write(AD7124_CH0_MAP_REG, ch_data, 2);

    // Disable internal reference for REFIN1 measurement
    ctrl_val = AD7124_ADC_CTRL_DATA_STATUS |
               AD7124_ADC_CTRL_POWER_MODE(0) |
               AD7124_ADC_CTRL_MODE(AD7124_MODE_SINGLE) |
               AD7124_ADC_CTRL_CLK_SEL(0);
    ctrl_data[0] = (ctrl_val >> 8) & 0xFF;
    ctrl_data[1] = ctrl_val & 0xFF;
    adc_reg_write(AD7124_ADC_CTRL_REG, ctrl_data, 2);

    // Wait for conversion
    status = 0x80;
    timeout = 2000;
    while ((status & 0x80) && timeout > 0) {
        sleep_ms(1);
        adc_reg_read(AD7124_STATUS_REG, &status, 1);
        timeout--;
    }

    if (timeout <= 0) {
        printf("REFIN1 measurement timeout!\n");
        g_vref_correction = 1.0f;
        return 0.0f;
    }

    adc_reg_read(AD7124_DATA_REG, data_bytes, 4);
    uint32_t adc_refin1 = ((uint32_t)data_bytes[0] << 16) |
                          ((uint32_t)data_bytes[1] << 8) |
                          data_bytes[2];

    printf("RTD1 ADC (REFIN1):   0x%06X (%u)\n", adc_refin1, adc_refin1);

    // Calculate V_REFIN1
    // ADC_refin1 = V_RTD / V_REFIN1 × 2^24
    // V_REFIN1 = V_RTD / (ADC_refin1 / 2^24) = V_RTD × 2^24 / ADC_refin1
    float v_refin1 = v_rtd * 16777216.0f / (float)adc_refin1;

    printf("Calculated V_REFIN1: %.4f V\n", v_refin1);
    printf("Expected V_REFIN1:   %.4f V (I_exc * R_ref)\n", expected_vref);

    // Calculate correction factor
    // If V_REFIN1 is higher than expected (due to leakage adding current),
    // our RTD readings will be lower than actual - we need to scale them up.
    // But wait - in ratiometric measurement, higher V_ref means lower ratio,
    // so we need: correction = V_REFIN1_measured / expected_vref
    // Actually no - let me think again...
    //
    // R_rtd = R_ref * (ADC_rtd / ADC_max)
    // If leakage adds current through R_ref but not RTD:
    //   V_ref = (I_exc + I_leak) * R_ref  (higher than expected)
    //   V_rtd = I_exc * R_rtd (normal)
    //   Ratio = V_rtd / V_ref = I_exc * R_rtd / ((I_exc + I_leak) * R_ref)
    //   This gives LOWER ratio, meaning we read LOWER resistance
    //
    // To correct: multiply by (I_exc + I_leak) / I_exc = V_ref_measured / V_ref_expected
    g_vref_correction = v_refin1 / expected_vref;

    printf("Leakage correction factor: %.6f\n", g_vref_correction);

    // Calculate implied leakage current
    float i_expected = expected_vref / 5030.0f;  // Using nominal R_ref
    float i_actual = v_refin1 / 5030.0f;
    float i_leakage = (i_actual - i_expected) * 1e6f;  // Convert to µA
    printf("Implied leakage current: %.2f µA\n", i_leakage);

    // === Restore Setup 0 to use REFIN1 for RTD measurements ===
    cfg_val = AD7124_CFG_REG_REF_BUFP |
              AD7124_CFG_REG_REF_BUFM |
              AD7124_CFG_REG_AIN_BUFP |
              AD7124_CFG_REG_AIN_BUFM |
              AD7124_CFG_REG_REF_SEL(AD7124_REFIN1) |
              AD7124_CFG_REG_PGA(AD7124_PGA_1);
    cfg_data[0] = (cfg_val >> 8) & 0xFF;
    cfg_data[1] = cfg_val & 0xFF;
    adc_reg_write(AD7124_CFG0_REG, cfg_data, 2);

    // Return to standby
    ctrl_val = AD7124_ADC_CTRL_DATA_STATUS |
               AD7124_ADC_CTRL_POWER_MODE(0) |
               AD7124_ADC_CTRL_MODE(AD7124_MODE_STANDBY) |
               AD7124_ADC_CTRL_CLK_SEL(0);
    ctrl_data[0] = (ctrl_val >> 8) & 0xFF;
    ctrl_data[1] = ctrl_val & 0xFF;
    adc_reg_write(AD7124_ADC_CTRL_REG, ctrl_data, 2);

    // Restore channel 0 to RTD1 configuration (AIN0/AIN1 per schematic)
    ch_val = AD7124_CH_MAP_REG_CH_ENABLE |
             AD7124_CH_MAP_REG_SETUP(0) |
             AD7124_CH_MAP_REG_AINP(AD7124_AIN0) |
             AD7124_CH_MAP_REG_AINM(AD7124_AIN1);
    ch_data[0] = (ch_val >> 8) & 0xFF;
    ch_data[1] = ch_val & 0xFF;
    adc_reg_write(AD7124_CH0_MAP_REG, ch_data, 2);

    printf("=== V_REFIN1 Measurement Complete ===\n\n");

    return v_refin1;
}