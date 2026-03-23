#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "ad1724.h"
#include "sdi12.h"
#include "adg708.h"
#include "nvm.h"
#include "calibration_mode.h"

// Pico W devices use a GPIO on the WIFI chip for the LED,
// so when building for Pico W, CYW43_WL_GPIO_LED_PIN will be defined
#ifdef CYW43_WL_GPIO_LED_PIN
#include "pico/cyw43_arch.h"
#endif

// SPI Defines
// We are going to use SPI 0, and allocate it to the following GPIO pins
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
spi_inst_t *SPI_PORT = spi0;
const uint PIN_MISO = 16;
const uint PIN_CS   = 17;
const uint PIN_SCK  = 18;
const uint PIN_MOSI = 19;

// SDI-12 Defines
const uint PIN_SDI12_DATA = 10; // GPIO10 for SDI-12 data line (needs level shifter to 5V)

// Global RTD config for SDI-12 callback
static rtd_config_t global_rtd_config;
static float last_temperature = 0.0f;

// Per-channel calibration data loaded from NVM at startup
static calibration_info_t g_cal_info;

// Multi-channel RTD storage (RTDs 1-7)
#define NUM_RTDS 7
// Support for RTDs 1-7

static float rtd_temperatures[NUM_RTDS] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
static float rtd_resistances[NUM_RTDS] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

// Helper: start a single conversion and wait for it to complete, then read result.
// Returns true on success, false on timeout.
static bool do_conversion(uint32_t *data, uint8_t *ch) {
    adc_start_single_conversion();
    uint8_t status;
    int timeout = 100;
    while (timeout > 0) {
        adc_reg_read(AD7124_STATUS_REG, &status, 1);
        if ((status & 0x80) == 0) {
            return adc_read_rtd_data(data, ch);
        }
        sleep_ms(10);
        timeout--;
    }
    return false;
}

// Perform LED initialisation
int pico_led_init(void) {
#if defined(PICO_DEFAULT_LED_PIN)
    // A device like Pico that uses a GPIO for the LED will define PICO_DEFAULT_LED_PIN
    // so we can use normal GPIO functionality to turn the led on and off
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    return PICO_OK;
#elif defined(CYW43_WL_GPIO_LED_PIN)
    // For Pico W devices we need to initialise the driver etc
    return cyw43_arch_init();
#endif
}

// Turn the led on or off
void pico_set_led(bool led_on) {
#if defined(PICO_DEFAULT_LED_PIN)
    // Just set the GPIO on or off
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
#elif defined(CYW43_WL_GPIO_LED_PIN)
    // Ask the wifi "driver" to set the GPIO on or off
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
#endif
}

// Calibration mode measurement function
// Called by calibration mode to measure a specific RTD channel
bool calibration_measure_rtd(uint8_t channel) {
    // Validate channel (1-7)
    if (channel < 1 || channel > 7) {
        printf("ERROR: Invalid channel %d (must be 1-7)\n", channel);
        return false;
    }

    uint8_t rtd_num = channel - 1;        // Convert to 0-based index
    uint8_t mux_channel = channel;        // Mux channels are 1-7
    uint8_t adc_channel_num = rtd_num;    // ADC channels are 0-6

    printf("Calibration measurement: RTD %d (Mux Ch%d, ADC Ch%d)\n",
           channel, mux_channel, adc_channel_num);

    // Enable ADC channel
    adc_enable_single_channel(adc_channel_num);

    // Switch mux
    if (!adg708_select_channel(mux_channel)) {
        printf("ERROR: Failed to select mux channel %d\n", mux_channel);
        return false;
    }

    // Wait for mux and analog front-end to settle after channel switch
    sleep_ms(20);

    // Discard 3 initial conversions to flush the ADC filter and allow residual
    // charge from the previous channel to dissipate (per technical review §7.2)
    printf("Discarding 3 initial conversions to flush ADC filter...\n");
    for (int i = 0; i < 3; i++) {
        uint32_t dummy_data;
        uint8_t dummy_channel;
        if (!do_conversion(&dummy_data, &dummy_channel)) {
            printf("ERROR: Dummy conversion %d timeout\n", i + 1);
            return false;
        }
    }

    // Take 3 readings and average for a stable calibration measurement.
    // Use NOMINAL r_ref so the Python calibration formula is mathematically correct.
    rtd_config_t cal_config = global_rtd_config;
    cal_config.r_ref = 5030.0f;  // Use nominal value, not calibrated value

    float resistance_sum = 0.0f;
    float temperature_sum = 0.0f;
    const int NUM_SAMPLES = 3;

    for (int i = 0; i < NUM_SAMPLES; i++) {
        uint32_t rtd_data;
        uint8_t read_channel;
        if (!do_conversion(&rtd_data, &read_channel)) {
            printf("ERROR: Conversion %d timeout\n", i + 1);
            return false;
        }
        resistance_sum += adc_calculate_resistance(rtd_data, &cal_config, rtd_num);
        temperature_sum += adc_calculate_temperature(rtd_data, &cal_config, rtd_num);
    }

    float resistance = resistance_sum / NUM_SAMPLES;
    float temperature = temperature_sum / NUM_SAMPLES;

    rtd_resistances[rtd_num] = resistance;
    rtd_temperatures[rtd_num] = temperature;

    printf("RTD %d: %.2fΩ, %.2f°C (avg of %d samples)\n",
           channel, resistance, temperature, NUM_SAMPLES);

    return true;
}

// SDI-12 measurement callback
// Called when the SDI-12 master requests a measurement
// measurement_index: 0 = aM!, 1 = aM1!, 2 = aM2!, 3 = aM3!, 8 = aM8!, 9 = aM9!
bool sdi12_measurement_callback(uint8_t measurement_index, sdi12_measurement_t *data) {
    printf("SDI-12 measurement requested for index %d\n", measurement_index);

    // Handle temperature sensor commands
    if (measurement_index == 8) {
        // M8! - Raspberry Pi Pico internal temperature
        printf("Reading Raspberry Pi Pico internal temperature\n");
        float temperature = pico_read_internal_temperature();

        if (temperature > -100.0f) {  // Valid temperature
            data->values[0] = temperature;
            data->num_values = 1;
            data->time_seconds = 0;
            printf("Pico Temperature: %.2f°C\n", temperature);
            return true;
        } else {
            printf("ERROR: Failed to read Pico temperature\n");
            return false;
        }
    } else if (measurement_index == 9) {
        // M9! - AD7124 ADC internal temperature
        printf("Reading AD7124 internal temperature\n");
        float temperature = adc_read_internal_temperature();

        if (temperature > -100.0f) {  // Valid temperature
            data->values[0] = temperature;
            data->num_values = 1;
            data->time_seconds = 0;
            printf("AD7124 Temperature: %.2f°C\n", temperature);
            return true;
        } else {
            printf("ERROR: Failed to read AD7124 temperature\n");
            return false;
        }
    }

    // Validate measurement index (1-7 for RTD1-RTD7)
    if (measurement_index < 1 || measurement_index > NUM_RTDS) {
        printf("Invalid measurement index: %d (valid: 1-7, 8-9)\n", measurement_index);
        return false;
    }

    // Get RTD number (convert from 1-based to 0-based index)
    uint8_t rtd_num = measurement_index - 1;
    uint8_t mux_channel = measurement_index; // Mux channels are 1-7
    uint8_t adc_channel_num = rtd_num;       // ADC channels are 0-6

    printf("Reading RTD %d (Mux Ch%d, ADC Ch%d)\n", measurement_index, mux_channel, adc_channel_num);

    // Step 1: Enable only this ADC channel
    adc_enable_single_channel(adc_channel_num);

    // Step 2: Switch mux to connect this RTD
    if (!adg708_select_channel(mux_channel)) {
        printf("ERROR: Failed to select mux channel %d\n", mux_channel);
        return false;
    }

    // Step 3: Wait for mux and analog front-end to settle after channel switch
    sleep_ms(20);

    // Step 4: Apply per-channel calibrated r_ref (§7.3 channel-specific calibration).
    // Each channel has slightly different effective path impedance, so a single
    // global r_ref is insufficient. Use the per-channel value stored in NVM.
    global_rtd_config.r_ref = g_cal_info.r_ref_calibrated[rtd_num];
    printf("RTD %d: using r_ref=%.2f ohms (channel-specific)\n",
           measurement_index, global_rtd_config.r_ref);

    // Step 5: Discard 3 initial conversions to flush the ADC sinc filter and allow
    // residual charge from the previous channel to dissipate (per technical review §7.2).
    // A single dummy read is insufficient because the sinc filter has multiple internal
    // stages that require several conversion cycles to fully reflect the new input.
    printf("Discarding 3 initial conversions to flush ADC filter...\n");
    for (int i = 0; i < 3; i++) {
        uint32_t dummy_data;
        uint8_t dummy_ch;
        if (!do_conversion(&dummy_data, &dummy_ch)) {
            printf("ERROR: Dummy conversion %d timeout for RTD %d\n", i + 1, measurement_index);
            return false;
        }
        printf("Dummy read %d: 0x%06X (discarded)\n", i + 1, dummy_data);
    }

    // Step 6: Take 8 readings and average to reduce noise and improve repeatability.
    float resistance_sum = 0.0f;
    float temperature_sum = 0.0f;
    const int NUM_SAMPLES = 8;

    for (int i = 0; i < NUM_SAMPLES; i++) {
        uint32_t rtd_data;
        uint8_t read_channel;
        if (!do_conversion(&rtd_data, &read_channel)) {
            printf("ERROR: Conversion %d timeout for RTD %d\n", i + 1, measurement_index);
            return false;
        }
        resistance_sum += adc_calculate_resistance(rtd_data, &global_rtd_config, rtd_num);
        temperature_sum += adc_calculate_temperature(rtd_data, &global_rtd_config, rtd_num);
    }

    float resistance = resistance_sum / NUM_SAMPLES;
    float temperature = temperature_sum / NUM_SAMPLES;

    // Store results and fill SDI-12 response
    rtd_resistances[rtd_num] = resistance;
    rtd_temperatures[rtd_num] = temperature;

    data->values[0] = temperature;
    data->num_values = 1;
    data->time_seconds = 0; // Data ready immediately

    printf("RTD %d: %.2fΩ, %.2f°C (avg of %d samples)\n",
           measurement_index, resistance, temperature, NUM_SAMPLES);

    return true;
}

int main(){
    stdio_init_all();

    // Initialize LED
    int rc = pico_led_init();
    hard_assert(rc == PICO_OK);

    // Initialize NVM (Non-Volatile Memory)
    printf("\n=== Initializing NVM ===\n");
    nvm_data_t nvm_data;
    bool nvm_valid = nvm_init();

    if (nvm_valid) {
        printf("NVM initialized successfully\n");
        nvm_read(&nvm_data);
    } else {
        printf("NVM using default values (first boot or corrupted data)\n");
        nvm_get_defaults(&nvm_data);
    }

    // Cache per-channel calibration data for use in measurement callbacks
    g_cal_info = nvm_data.calibration;

    // Initialize ADG708 analog mux
    printf("\n=== Initializing ADG708 Analog Mux ===\n");
    adg708_config_t mux_config = {
        .pin_en = 20,
        .pin_a0 = 21,
        .pin_a1 = 22,
        .pin_a2 = 23
    };
    adg708_init(&mux_config);

    // SPI initialisation. ADCs often need slower speeds and specific modes
    spi_init(SPI_PORT, 1000*1000);

    // Set SPI mode 0 (CPOL=0, CPHA=0) which is common for ADCs
    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS,   GPIO_FUNC_SIO);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    
    // Chip select is active-low, so we'll initialise it to a driven-high state
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);
    // For more examples of SPI use see https://github.com/raspberrypi/pico-examples/tree/master/spi

    // Check ADC communication
    bool comms_ok = adc_verify_communication();

    if (!comms_ok) {

        while (true) {
            // Slow blink for bad communication
            printf("AD7124 communication failed!\n");
            pico_set_led(true);
            sleep_ms(1000);
            pico_set_led(false);
            sleep_ms(1000);
        }
    }

    printf("AD7124 communication OK\n");

    // Configure RTD measurement (PT100, 4-wire ratiometric configuration)
    // Note: The ADC channels are already configured for all 3 RTDs in adc_configure_rtd()
    // Use calibrated values from NVM (default to 5030Ω if uncalibrated)
    global_rtd_config.r_ref = nvm_data.calibration.r_ref_calibrated[0];  // Use calibrated reference resistor
    global_rtd_config.r_rtd_0 = 100.0f;          // PT100 RTD (100Ω at 0°C) - using potentiometer for test
    global_rtd_config.alpha = nvm_data.calibration.temp_coeff;  // Use calibrated temperature coefficient
    global_rtd_config.excitation_current = AD7124_IOUT_500UA;  // 500µA excitation current (max safe with 5kΩ R_ref)
    global_rtd_config.rtd_ainp = AD7124_AIN1;    // Dummy - not used for multi-channel
    global_rtd_config.rtd_ainm = AD7124_AIN2;    // Dummy - not used for multi-channel
    global_rtd_config.ref_ainp = AD7124_AIN0;    // Reference positive (across RREF1)
    global_rtd_config.ref_ainm = AD7124_AIN1;    // Reference negative (across RREF1)
    global_rtd_config.iout_pin = AD7124_AIN0;    // Excitation current output on AIN0

    printf("Using NVM calibration data:\n");
    printf("  R_ref: %.2f ohms\n", global_rtd_config.r_ref);
    printf("  Alpha: %.8f\n", global_rtd_config.alpha);
    printf("  ADC offset: %.2f\n", nvm_data.calibration.adc_offset);
    printf("  ADC gain: %.6f\n", nvm_data.calibration.adc_gain);

    printf("Configuring RTD measurement system...\n");
    bool rtd_config_ok = adc_configure_rtd(&global_rtd_config);

    if (!rtd_config_ok) {
        printf("RTD configuration failed!\n");
        while (true) {
            pico_set_led(true);
            sleep_ms(200);
            pico_set_led(false);
            sleep_ms(200);
        }
    }

    printf("RTD configuration complete.\n");
    printf("Will cycle through RTDs 1-7 using ADG708 mux and ADC channels\n");

    // Initialize SDI-12 sensor interface
    printf("\n=== Initializing SDI-12 Sensor ===\n");
    sdi12_sensor_info_t sensor_info = {
        .address = '0',                     // Default address: 0
        .sdi_version = "14",                // SDI-12 version 1.4
        .vendor_id = "GEMS",            // Vendor ID (8 chars)
        .sensor_model = "GORGON",           // Model (6 chars)
        .sensor_version = "1.0",            // Version (3 chars) - will be updated from NVM
        .serial_number = "001"              // Serial number (will be updated from NVM)
    };

    // Update serial number from NVM if available
    if (strlen(nvm_data.manufacturing.device_serial) > 0 &&
        strcmp(nvm_data.manufacturing.device_serial, "UNCALIBRATED") != 0) {
        // Use first 3 chars of NVM serial (SDI-12 serial is limited to 3 chars)
        strncpy(sensor_info.serial_number, nvm_data.manufacturing.device_serial, 3);
        sensor_info.serial_number[3] = '\0';
    }

    // Update firmware version from NVM if available
    if (strlen(nvm_data.manufacturing.firmware_version) > 0 &&
        strcmp(nvm_data.manufacturing.firmware_version, "0.0") != 0) {
        // Use first 3 chars of firmware version (SDI-12 version is limited to 3 chars)
        strncpy(sensor_info.sensor_version, nvm_data.manufacturing.firmware_version, 3);
        sensor_info.sensor_version[3] = '\0';
    }

    printf("SDI-12 Sensor Configuration:\n");
    printf("  Address:    %c\n", sensor_info.address);
    printf("  Vendor:     %s\n", sensor_info.vendor_id);
    printf("  Model:      %s\n", sensor_info.sensor_model);
    printf("  Version:    %s (from NVM FW version)\n", sensor_info.sensor_version);
    printf("  Serial:     %s (from NVM serial)\n", sensor_info.serial_number);

    if (!sdi12_init(PIN_SDI12_DATA, &sensor_info)) {
        printf("SDI-12 initialization failed!\n");
        while (true) {
            pico_set_led(true);
            sleep_ms(100);
            pico_set_led(false);
            sleep_ms(100);
        }
    }

    // Register measurement callback
    sdi12_set_measurement_callback(sdi12_measurement_callback);

    // Initialize calibration mode
    printf("\n=== Initializing Calibration Mode ===\n");
    cal_mode_init();
    cal_mode_set_measurement_callback(calibration_measure_rtd);
    printf("Calibration mode ready\n");

    printf("\nSDI-12 sensor ready!\n");
    printf("Address: %c\n", sensor_info.address);
    printf("Send SDI-12 commands on GPIO%d (requires 3.3V <-> 5V level shifter)\n", PIN_SDI12_DATA);
    printf("\nSupported commands:\n");
    printf("  0I!    - Identify sensor\n");
    printf("  0M1!   - Measure RTD1 temperature\n");
    printf("  0M2!   - Measure RTD2 temperature\n");
    printf("  0M3!   - Measure RTD3 temperature\n");
    printf("  0M4!   - Measure RTD4 temperature\n");
    printf("  0M5!   - Measure RTD5 temperature\n");
    printf("  0M6!   - Measure RTD6 temperature\n");
    printf("  0M7!   - Measure RTD7 temperature\n");
    printf("  0M8!   - Measure Raspberry Pi Pico internal temperature\n");
    printf("  0M9!   - Measure AD7124 ADC internal temperature\n");
    printf("  0D0!   - Get measurement data (after aM command)\n");
    printf("\n=== NVM Debug Commands ===\n");
    printf("  nvm_dump           - Display all NVM data\n");
    printf("  nvm_reset          - Factory reset NVM\n");
    printf("  nvm_set_sn <sn>    - Set device serial number\n");
    printf("  nvm_set_date <YY-MM-DD> - Set manufacturing date\n");
    printf("  nvm_set_board <rev> - Set board revision\n");
    printf("  nvm_cal_rref <ch> <val> - Set R_ref calibration (ch 0-6)\n");
    printf("  nvm_cal_offset <ch> <val> - Set offset calibration (ch 0-6)\n");
    printf("  nvm_cal_scale <ch> <val> - Set scale calibration (ch 0-6)\n");
    printf("  nvm_set_hw_ver <ver> - Set hardware version\n");
    printf("  nvm_set_fw_ver <ver> - Set firmware version\n");
    printf("\n=== Calibration Mode Commands ===\n");
    printf("  cal_mode_start       - Enter calibration mode\n");
    printf("  cal_mode_stop        - Exit calibration mode\n");
    printf("  cal_set_serial <sn>  - Set serial (cal mode)\n");
    printf("  cal_set_hw_ver <ver> - Set HW version (cal mode)\n");
    printf("  cal_set_fw_ver <ver> - Set FW version (cal mode)\n");
    printf("  cal_set_date <date>  - Set date (cal mode)\n");
    printf("  cal_measure <ch>     - Measure channel (cal mode)\n");
    printf("\n=== Main Loop: SDI-12 Command Processing ===\n");
    printf("RTD measurements are only taken when requested via SDI-12\n");

    // Main loop - handle SDI-12 commands and check for UART input
    char cmd_buffer[128];
    int cmd_idx = 0;

    while (true) {
        // Process SDI-12 commands (only if not in calibration mode)
        if (!cal_mode_is_active()) {
            sdi12_task();
        }

        // Check for UART commands (non-blocking)
        int c = getchar_timeout_us(0);
        if (c != PICO_ERROR_TIMEOUT) {
            if (c == '\n' || c == '\r') {
                if (cmd_idx > 0) {
                    cmd_buffer[cmd_idx] = '\0';

                    // Process NVM commands
                    if (strcmp(cmd_buffer, "nvm_dump") == 0) {
                        nvm_cmd_dump();
                    } else if (strcmp(cmd_buffer, "nvm_reset") == 0) {
                        nvm_cmd_reset();
                    } else if (strncmp(cmd_buffer, "nvm_set_sn ", 11) == 0) {
                        nvm_cmd_set_serial(cmd_buffer + 11);
                    } else if (strncmp(cmd_buffer, "nvm_set_date ", 13) == 0) {
                        nvm_cmd_set_date(cmd_buffer + 13);
                    } else if (strncmp(cmd_buffer, "nvm_set_board ", 14) == 0) {
                        nvm_cmd_set_board(cmd_buffer + 14);
                    } else if (strncmp(cmd_buffer, "nvm_cal_rref ", 13) == 0) {
                        int ch;
                        float val;
                        if (sscanf(cmd_buffer + 13, "%d %f", &ch, &val) == 2) {
                            nvm_cmd_set_rref(ch, val);
                        } else {
                            printf("Usage: nvm_cal_rref <channel> <value>\n");
                        }
                    } else if (strncmp(cmd_buffer, "nvm_cal_offset ", 15) == 0) {
                        int ch;
                        float val;
                        if (sscanf(cmd_buffer + 15, "%d %f", &ch, &val) == 2) {
                            nvm_cmd_set_offset(ch, val);
                        } else {
                            printf("Usage: nvm_cal_offset <channel> <value>\n");
                        }
                    } else if (strncmp(cmd_buffer, "nvm_cal_scale ", 14) == 0) {
                        int ch;
                        float val;
                        if (sscanf(cmd_buffer + 14, "%d %f", &ch, &val) == 2) {
                            nvm_cmd_set_scale(ch, val);
                        } else {
                            printf("Usage: nvm_cal_scale <channel> <value>\n");
                        }
                    } else if (strncmp(cmd_buffer, "nvm_set_hw_ver ", 15) == 0) {
                        nvm_cmd_set_hw_version(cmd_buffer + 15);
                    } else if (strncmp(cmd_buffer, "nvm_set_fw_ver ", 15) == 0) {
                        nvm_cmd_set_fw_version(cmd_buffer + 15);
                    }
                    // Calibration mode commands
                    else if (strcmp(cmd_buffer, "cal_mode_start") == 0) {
                        cal_mode_enter();
                    } else if (strcmp(cmd_buffer, "cal_mode_stop") == 0) {
                        cal_mode_exit();
                    } else if (strncmp(cmd_buffer, "cal_set_serial ", 15) == 0) {
                        if (cal_mode_is_active()) {
                            nvm_cmd_set_serial(cmd_buffer + 15);
                        } else {
                            printf("ERROR: Not in calibration mode\n");
                        }
                    } else if (strncmp(cmd_buffer, "cal_set_hw_ver ", 15) == 0) {
                        if (cal_mode_is_active()) {
                            nvm_cmd_set_hw_version(cmd_buffer + 15);
                        } else {
                            printf("ERROR: Not in calibration mode\n");
                        }
                    } else if (strncmp(cmd_buffer, "cal_set_fw_ver ", 15) == 0) {
                        if (cal_mode_is_active()) {
                            nvm_cmd_set_fw_version(cmd_buffer + 15);
                        } else {
                            printf("ERROR: Not in calibration mode\n");
                        }
                    } else if (strncmp(cmd_buffer, "cal_set_date ", 13) == 0) {
                        if (cal_mode_is_active()) {
                            nvm_cmd_set_date(cmd_buffer + 13);
                        } else {
                            printf("ERROR: Not in calibration mode\n");
                        }
                    } else if (strncmp(cmd_buffer, "cal_measure ", 12) == 0) {
                        if (cal_mode_is_active()) {
                            int channel;
                            if (sscanf(cmd_buffer + 12, "%d", &channel) == 1) {
                                cal_mode_measure_channel(channel);
                            } else {
                                printf("Usage: cal_measure <channel>\n");
                            }
                        } else {
                            printf("ERROR: Not in calibration mode\n");
                        }
                    }

                    cmd_idx = 0;
                }
            } else if (cmd_idx < sizeof(cmd_buffer) - 1) {
                cmd_buffer[cmd_idx++] = (char)c;
            }
        }

        // Small delay to prevent busy-waiting
        sleep_ms(1);
    }
}