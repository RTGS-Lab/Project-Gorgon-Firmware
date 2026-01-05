/**
 * @file calibration_mode.c
 * @brief Calibration mode implementation
 */

#include "calibration_mode.h"
#include "nvm.h"
#include <stdio.h>

// Calibration mode state
static bool calibration_mode_active = false;

// External measurement callback function
// This will be set by the main application
static bool (*measurement_callback)(uint8_t channel) = NULL;

/**
 * Initialize calibration mode subsystem
 */
void cal_mode_init(void) {
    calibration_mode_active = false;
    measurement_callback = NULL;
    printf("Calibration mode subsystem initialized\n");
}

/**
 * Set measurement callback function
 * This should be called by main.c to provide access to the measurement function
 */
void cal_mode_set_measurement_callback(bool (*callback)(uint8_t)) {
    measurement_callback = callback;
}

/**
 * Check if device is in calibration mode
 */
bool cal_mode_is_active(void) {
    return calibration_mode_active;
}

/**
 * Enter calibration mode
 */
void cal_mode_enter(void) {
    printf("\n");
    printf("========================================\n");
    printf("ENTERING CALIBRATION MODE\n");
    printf("========================================\n");
    printf("SDI-12 interface disabled\n");
    printf("Calibration commands enabled\n");
    printf("\nCalibration Commands:\n");
    printf("  cal_set_serial <sn>      - Set device serial number\n");
    printf("  cal_set_hw_ver <ver>     - Set hardware version\n");
    printf("  cal_set_fw_ver <ver>     - Set firmware version\n");
    printf("  cal_set_date <YYYY-MM-DD> - Set manufacturing date\n");
    printf("  cal_measure <channel>    - Measure RTD channel (1-7)\n");
    printf("  cal_mode_stop            - Exit calibration mode\n");
    printf("========================================\n\n");

    calibration_mode_active = true;
}

/**
 * Exit calibration mode
 */
void cal_mode_exit(void) {
    printf("\n");
    printf("========================================\n");
    printf("EXITING CALIBRATION MODE\n");
    printf("========================================\n");
    printf("SDI-12 interface re-enabled\n");
    printf("Calibration commands disabled\n");
    printf("========================================\n\n");

    calibration_mode_active = false;
}

/**
 * Measure a specific RTD channel
 */
bool cal_mode_measure_channel(uint8_t channel) {
    if (!calibration_mode_active) {
        printf("ERROR: Not in calibration mode\n");
        return false;
    }

    if (channel < 1 || channel > 7) {
        printf("ERROR: Invalid channel %d (must be 1-7)\n", channel);
        return false;
    }

    if (measurement_callback == NULL) {
        printf("ERROR: Measurement callback not set\n");
        return false;
    }

    // Call the measurement callback (provided by main application)
    printf("Measuring RTD channel %d...\n", channel);
    return measurement_callback(channel);
}
