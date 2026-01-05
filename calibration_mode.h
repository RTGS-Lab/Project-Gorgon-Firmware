/**
 * @file calibration_mode.h
 * @brief Calibration mode support for production testing
 *
 * This module provides a calibration mode that allows automated testing
 * and calibration of the device via UART commands.
 */

#ifndef CALIBRATION_MODE_H
#define CALIBRATION_MODE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Initialize calibration mode subsystem
 */
void cal_mode_init(void);

/**
 * Set measurement callback function
 * This should be called by main.c to provide access to the measurement function
 * @param callback Function pointer to measurement callback
 */
void cal_mode_set_measurement_callback(bool (*callback)(uint8_t));

/**
 * Check if device is in calibration mode
 * @return true if in calibration mode, false otherwise
 */
bool cal_mode_is_active(void);

/**
 * Enter calibration mode
 * Disables SDI-12 command processing and enables calibration commands
 */
void cal_mode_enter(void);

/**
 * Exit calibration mode
 * Re-enables SDI-12 command processing
 */
void cal_mode_exit(void);

/**
 * Measure a specific RTD channel (used by calibration script)
 * @param channel Channel number (1-7)
 * @return true on success, false on failure
 */
bool cal_mode_measure_channel(uint8_t channel);

#endif // CALIBRATION_MODE_H
