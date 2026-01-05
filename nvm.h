/**
 * @file nvm.h
 * @brief Non-Volatile Memory (NVM) module for RP2040 QSPI flash storage
 *
 * This module provides persistent storage for manufacturing data, calibration
 * constants, and device serial numbers using the RP2040's external QSPI flash.
 *
 * Memory Layout:
 * - Flash offset: 0x1F0000 (last 64KB of 2MB flash)
 * - Sector size: 4KB (minimum erase unit)
 * - Data size: ~200 bytes (fits in single 256-byte page)
 *
 * Data Integrity:
 * - CRC32 checksum for validation
 * - Magic number for identification
 * - Version number for future compatibility
 * - Graceful fallback to defaults on corruption
 */

#ifndef NVM_H
#define NVM_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

// Flash configuration
#define NVM_FLASH_OFFSET    0x001F0000  // Offset from XIP_BASE (1984KB into flash)
#define NVM_SECTOR_SIZE     FLASH_SECTOR_SIZE  // 4096 bytes
#define NVM_PAGE_SIZE       FLASH_PAGE_SIZE    // 256 bytes

// Data validation
#define NVM_MAGIC_NUMBER    0x5344494D  // "SDIM" in ASCII hex
#define NVM_VERSION         1           // Structure version
#define NVM_MAX_SERIAL_LEN  16          // Maximum serial number length

/**
 * Manufacturing information structure
 */
typedef struct {
    uint32_t year;                              // Manufacturing year (e.g., 2025)
    uint8_t month;                              // Month (1-12)
    uint8_t day;                                // Day (1-31)
    uint8_t reserved;                           // Padding for alignment
    char device_serial[NVM_MAX_SERIAL_LEN];     // Device serial number
    char board_revision[NVM_MAX_SERIAL_LEN];    // Board revision/batch number
    char hardware_version[NVM_MAX_SERIAL_LEN];  // Hardware version (e.g., "1.0")
    char firmware_version[NVM_MAX_SERIAL_LEN];  // Firmware version (e.g., "1.0")
} manufacturing_info_t;

/**
 * Calibration constants structure
 */
typedef struct {
    // RTD calibration (7 channels)
    float r_ref_calibrated[7];      // Calibrated reference resistor values (ohms)
    float rtd_offset[7];            // Offset calibration for each RTD (ohms)
    float rtd_scale[7];             // Scale factor for each RTD (unitless)

    // ADC calibration
    float adc_offset;               // ADC offset calibration (counts)
    float adc_gain;                 // ADC gain calibration (unitless)

    // Temperature calibration
    float temp_coeff;               // RTD temperature coefficient (ohms/ohm/°C)

    uint32_t calibration_date;      // Unix timestamp of last calibration
} calibration_info_t;

/**
 * Complete NVM data structure (stored in flash)
 * Total size: ~200 bytes (fits in single 256-byte page)
 */
typedef struct {
    // Header (for validation)
    uint32_t magic;                 // Magic number (NVM_MAGIC_NUMBER)
    uint16_t version;               // Structure version (NVM_VERSION)
    uint16_t reserved;              // Padding for alignment

    // Manufacturing information
    manufacturing_info_t manufacturing;

    // Calibration constants
    calibration_info_t calibration;

    // Footer (for integrity)
    uint32_t crc32;                 // CRC32 of all data above this field

} __attribute__((packed)) nvm_data_t;

/**
 * Initialize the NVM module
 *
 * Reads and validates data from flash. If valid data exists, it is cached
 * for fast subsequent reads. If no valid data exists (first boot), default
 * values are loaded.
 *
 * @return true if valid data was found in flash, false if using defaults
 */
bool nvm_init(void);

/**
 * Read complete NVM data structure
 *
 * Returns cached data if available, otherwise reads from flash.
 *
 * @param data Pointer to structure to fill with NVM data
 * @return true on success, false on failure (NULL pointer)
 */
bool nvm_read(nvm_data_t *data);

/**
 * Read only manufacturing information
 *
 * @param info Pointer to structure to fill with manufacturing data
 * @return true on success, false on failure
 */
bool nvm_read_manufacturing(manufacturing_info_t *info);

/**
 * Read only calibration information
 *
 * @param cal Pointer to structure to fill with calibration data
 * @return true on success, false on failure
 */
bool nvm_read_calibration(calibration_info_t *cal);

/**
 * Write complete NVM data structure to flash
 *
 * Erases the NVM sector and writes new data. Automatically calculates and
 * sets the CRC32, magic number, and version fields.
 *
 * WARNING: This disables interrupts during flash operations (handled by SDK)
 *
 * @param data Pointer to data structure to write (magic/version/crc32 will be set)
 * @return true on success (with verification), false on failure
 */
bool nvm_write(const nvm_data_t *data);

/**
 * Update only manufacturing information in flash
 *
 * Reads current data, updates manufacturing fields, and writes back.
 *
 * @param info Pointer to new manufacturing data
 * @return true on success, false on failure
 */
bool nvm_write_manufacturing(const manufacturing_info_t *info);

/**
 * Update only calibration information in flash
 *
 * Reads current data, updates calibration fields, and writes back.
 *
 * @param cal Pointer to new calibration data
 * @return true on success, false on failure
 */
bool nvm_write_calibration(const calibration_info_t *cal);

/**
 * Check if valid NVM data exists in flash
 *
 * Validates magic number, version, and CRC32 without caching data.
 *
 * @return true if valid data exists, false otherwise
 */
bool nvm_is_valid(void);

/**
 * Perform factory reset (write default values to flash)
 *
 * Writes uncalibrated default values to NVM.
 *
 * @return true on success, false on failure
 */
bool nvm_factory_reset(void);

/**
 * Calculate CRC32 for NVM data structure
 *
 * Calculates CRC32 over entire structure except the crc32 field itself.
 *
 * @param data Pointer to NVM data structure
 * @return CRC32 value
 */
uint32_t nvm_calculate_crc(const nvm_data_t *data);

/**
 * Get default (uncalibrated) NVM data
 *
 * Fills structure with factory default values.
 *
 * @param data Pointer to structure to fill with defaults
 */
void nvm_get_defaults(nvm_data_t *data);

// =============================================================================
// UART Debug Command Functions
// =============================================================================

/**
 * Display all NVM data in human-readable format
 */
void nvm_cmd_dump(void);

/**
 * Set device serial number
 * @param serial Serial number string
 * @return true on success, false on failure
 */
bool nvm_cmd_set_serial(const char *serial);

/**
 * Set manufacturing date
 * @param date_str Date string in format YYYY-MM-DD
 * @return true on success, false on failure
 */
bool nvm_cmd_set_date(const char *date_str);

/**
 * Set board revision
 * @param board Board revision string
 * @return true on success, false on failure
 */
bool nvm_cmd_set_board(const char *board);

/**
 * Factory reset command
 * @return true on success, false on failure
 */
bool nvm_cmd_reset(void);

/**
 * Set calibration R_ref value for a channel
 * @param channel Channel number (0-6)
 * @param value R_ref value in ohms
 * @return true on success, false on failure
 */
bool nvm_cmd_set_rref(int channel, float value);

/**
 * Set calibration offset for a channel
 * @param channel Channel number (0-6)
 * @param value Offset value in ohms
 * @return true on success, false on failure
 */
bool nvm_cmd_set_offset(int channel, float value);

/**
 * Set calibration scale for a channel
 * @param channel Channel number (0-6)
 * @param value Scale factor (unitless)
 * @return true on success, false on failure
 */
bool nvm_cmd_set_scale(int channel, float value);

/**
 * Set hardware version
 * @param version Hardware version string
 * @return true on success, false on failure
 */
bool nvm_cmd_set_hw_version(const char *version);

/**
 * Set firmware version
 * @param version Firmware version string
 * @return true on success, false on failure
 */
bool nvm_cmd_set_fw_version(const char *version);

#endif // NVM_H
