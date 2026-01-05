/**
 * @file nvm.c
 * @brief Non-Volatile Memory (NVM) module implementation
 */

#include "nvm.h"
#include <stdio.h>

// Private state
static nvm_data_t nvm_cache;            // Cached NVM data for fast reads
static bool nvm_initialized = false;    // Initialization flag
static bool nvm_cache_valid = false;    // Cache validity flag

// Private function prototypes
static uint32_t nvm_calc_crc32(const void *data, size_t len);
static bool nvm_validate_structure(const nvm_data_t *data);
static const nvm_data_t* nvm_get_flash_ptr(void);

/**
 * Get pointer to NVM data in flash (via XIP)
 */
static const nvm_data_t* nvm_get_flash_ptr(void) {
    return (const nvm_data_t *)(XIP_BASE + NVM_FLASH_OFFSET);
}

/**
 * Calculate CRC32 using standard polynomial
 * CRC-32 (ISO 3309, ITU-T V.42, Ethernet, PNG, etc.)
 */
static uint32_t nvm_calc_crc32(const void *data, size_t len) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < len; i++) {
        crc ^= bytes[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }

    return ~crc;
}

/**
 * Validate NVM data structure
 */
static bool nvm_validate_structure(const nvm_data_t *data) {
    if (data == NULL) {
        printf("NVM validation failed: NULL pointer\n");
        return false;
    }

    // Check magic number
    if (data->magic != NVM_MAGIC_NUMBER) {
        printf("NVM validation failed: bad magic (0x%08X, expected 0x%08X)\n",
               data->magic, NVM_MAGIC_NUMBER);
        return false;
    }

    // Check version
    if (data->version != NVM_VERSION) {
        printf("NVM validation failed: bad version (%d, expected %d)\n",
               data->version, NVM_VERSION);
        return false;
    }

    // Verify CRC32
    size_t crc_len = offsetof(nvm_data_t, crc32);
    uint32_t calculated_crc = nvm_calc_crc32(data, crc_len);
    if (data->crc32 != calculated_crc) {
        printf("NVM validation failed: CRC mismatch (0x%08X, expected 0x%08X)\n",
               data->crc32, calculated_crc);
        return false;
    }

    return true;
}

/**
 * Get default (uncalibrated) NVM data
 */
void nvm_get_defaults(nvm_data_t *data) {
    if (data == NULL) return;

    memset(data, 0, sizeof(nvm_data_t));

    // Set header
    data->magic = NVM_MAGIC_NUMBER;
    data->version = NVM_VERSION;

    // Manufacturing defaults
    data->manufacturing.year = 0;
    data->manufacturing.month = 0;
    data->manufacturing.day = 0;
    strncpy(data->manufacturing.device_serial, "UNCALIBRATED", NVM_MAX_SERIAL_LEN - 1);
    strncpy(data->manufacturing.board_revision, "UNKNOWN", NVM_MAX_SERIAL_LEN - 1);
    strncpy(data->manufacturing.hardware_version, "0.0", NVM_MAX_SERIAL_LEN - 1);
    strncpy(data->manufacturing.firmware_version, "0.0", NVM_MAX_SERIAL_LEN - 1);

    // Calibration defaults (typical PT100/PT1000 values)
    for (int i = 0; i < 7; i++) {
        data->calibration.r_ref_calibrated[i] = 5030.0f;  // 5030 ohm reference
        data->calibration.rtd_offset[i] = 0.0f;
        data->calibration.rtd_scale[i] = 1.0f;
    }
    data->calibration.adc_offset = 0.0f;
    data->calibration.adc_gain = 1.0f;
    data->calibration.temp_coeff = 0.00385f;  // PT100 alpha coefficient
    data->calibration.calibration_date = 0;

    // Calculate CRC
    data->crc32 = nvm_calculate_crc(data);
}

/**
 * Calculate CRC32 for NVM data structure
 */
uint32_t nvm_calculate_crc(const nvm_data_t *data) {
    if (data == NULL) return 0;
    size_t crc_len = offsetof(nvm_data_t, crc32);
    return nvm_calc_crc32(data, crc_len);
}

/**
 * Check if valid NVM data exists in flash
 */
bool nvm_is_valid(void) {
    const nvm_data_t *flash_data = nvm_get_flash_ptr();
    return nvm_validate_structure(flash_data);
}

/**
 * Initialize the NVM module
 */
bool nvm_init(void) {
    printf("\n=== Initializing NVM Module ===\n");
    printf("Flash offset: 0x%08X\n", NVM_FLASH_OFFSET);
    printf("Flash address: 0x%08X\n", (uint32_t)(XIP_BASE + NVM_FLASH_OFFSET));
    printf("NVM data size: %zu bytes\n", sizeof(nvm_data_t));

    // Read data from flash
    const nvm_data_t *flash_data = nvm_get_flash_ptr();

    // Validate and cache
    if (nvm_validate_structure(flash_data)) {
        printf("Valid NVM data found in flash\n");
        memcpy(&nvm_cache, flash_data, sizeof(nvm_data_t));
        nvm_cache_valid = true;
        nvm_initialized = true;

        // Display summary
        printf("Device Serial: %s\n", nvm_cache.manufacturing.device_serial);
        printf("Board Revision: %s\n", nvm_cache.manufacturing.board_revision);
        printf("Mfg Date: %04u-%02u-%02u\n",
               nvm_cache.manufacturing.year,
               nvm_cache.manufacturing.month,
               nvm_cache.manufacturing.day);
        printf("Cal Date: %u\n", nvm_cache.calibration.calibration_date);

        return true;
    } else {
        printf("No valid NVM data found - using defaults\n");
        nvm_get_defaults(&nvm_cache);
        nvm_cache_valid = true;
        nvm_initialized = true;
        return false;
    }
}

/**
 * Read complete NVM data structure
 */
bool nvm_read(nvm_data_t *data) {
    if (data == NULL) {
        printf("NVM read failed: NULL pointer\n");
        return false;
    }

    if (!nvm_initialized) {
        printf("NVM read failed: not initialized\n");
        return false;
    }

    memcpy(data, &nvm_cache, sizeof(nvm_data_t));
    return true;
}

/**
 * Read only manufacturing information
 */
bool nvm_read_manufacturing(manufacturing_info_t *info) {
    if (info == NULL) {
        printf("NVM read manufacturing failed: NULL pointer\n");
        return false;
    }

    if (!nvm_initialized) {
        printf("NVM read manufacturing failed: not initialized\n");
        return false;
    }

    memcpy(info, &nvm_cache.manufacturing, sizeof(manufacturing_info_t));
    return true;
}

/**
 * Read only calibration information
 */
bool nvm_read_calibration(calibration_info_t *cal) {
    if (cal == NULL) {
        printf("NVM read calibration failed: NULL pointer\n");
        return false;
    }

    if (!nvm_initialized) {
        printf("NVM read calibration failed: not initialized\n");
        return false;
    }

    memcpy(cal, &nvm_cache.calibration, sizeof(calibration_info_t));
    return true;
}

/**
 * Write complete NVM data structure to flash
 */
bool nvm_write(const nvm_data_t *data) {
    if (data == NULL) {
        printf("NVM write failed: NULL pointer\n");
        return false;
    }

    printf("\n=== Writing NVM Data ===\n");

    // Prepare data with correct header and CRC
    uint8_t page_buffer[NVM_PAGE_SIZE];
    memset(page_buffer, 0xFF, NVM_PAGE_SIZE);  // Flash erase state

    nvm_data_t write_data;
    memcpy(&write_data, data, sizeof(nvm_data_t));

    // Set header fields
    write_data.magic = NVM_MAGIC_NUMBER;
    write_data.version = NVM_VERSION;

    // Calculate CRC
    write_data.crc32 = nvm_calculate_crc(&write_data);

    // Copy to page buffer
    memcpy(page_buffer, &write_data, sizeof(nvm_data_t));

    printf("Data prepared, size: %zu bytes\n", sizeof(nvm_data_t));
    printf("CRC32: 0x%08X\n", write_data.crc32);

    // Erase sector (4KB)
    printf("Erasing flash sector at offset 0x%08X...\n", NVM_FLASH_OFFSET);
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(NVM_FLASH_OFFSET, NVM_SECTOR_SIZE);
    restore_interrupts(ints);
    printf("Sector erased\n");

    // Small delay to ensure flash is ready
    sleep_ms(1);

    // Write page (256 bytes)
    printf("Writing flash page at offset 0x%08X...\n", NVM_FLASH_OFFSET);
    ints = save_and_disable_interrupts();
    flash_range_program(NVM_FLASH_OFFSET, page_buffer, NVM_PAGE_SIZE);
    restore_interrupts(ints);
    printf("Page written\n");

    // Small delay to ensure flash is ready
    sleep_ms(1);

    // Verify write
    const nvm_data_t *flash_data = nvm_get_flash_ptr();
    if (nvm_validate_structure(flash_data)) {
        // Double-check with byte-by-byte comparison
        if (memcmp(flash_data, &write_data, sizeof(nvm_data_t)) == 0) {
            printf("NVM write verified successfully\n");

            // Update cache
            memcpy(&nvm_cache, &write_data, sizeof(nvm_data_t));
            nvm_cache_valid = true;
            nvm_initialized = true;

            return true;
        } else {
            printf("ERROR: NVM data mismatch after write\n");
            return false;
        }
    } else {
        printf("ERROR: NVM validation failed after write\n");
        return false;
    }
}

/**
 * Update only manufacturing information in flash
 */
bool nvm_write_manufacturing(const manufacturing_info_t *info) {
    if (info == NULL) {
        printf("NVM write manufacturing failed: NULL pointer\n");
        return false;
    }

    if (!nvm_initialized) {
        printf("NVM write manufacturing failed: not initialized\n");
        return false;
    }

    printf("Updating manufacturing information...\n");

    // Get current data
    nvm_data_t write_data;
    memcpy(&write_data, &nvm_cache, sizeof(nvm_data_t));

    // Update manufacturing fields
    memcpy(&write_data.manufacturing, info, sizeof(manufacturing_info_t));

    // Write back
    return nvm_write(&write_data);
}

/**
 * Update only calibration information in flash
 */
bool nvm_write_calibration(const calibration_info_t *cal) {
    if (cal == NULL) {
        printf("NVM write calibration failed: NULL pointer\n");
        return false;
    }

    if (!nvm_initialized) {
        printf("NVM write calibration failed: not initialized\n");
        return false;
    }

    printf("Updating calibration information...\n");

    // Get current data
    nvm_data_t write_data;
    memcpy(&write_data, &nvm_cache, sizeof(nvm_data_t));

    // Update calibration fields
    memcpy(&write_data.calibration, cal, sizeof(calibration_info_t));

    // Write back
    return nvm_write(&write_data);
}

/**
 * Perform factory reset (write default values to flash)
 */
bool nvm_factory_reset(void) {
    printf("\n=== NVM Factory Reset ===\n");

    nvm_data_t defaults;
    nvm_get_defaults(&defaults);

    return nvm_write(&defaults);
}

// =============================================================================
// UART Debug Commands
// =============================================================================

/**
 * Display all NVM data in human-readable format
 */
void nvm_cmd_dump(void) {
    if (!nvm_initialized) {
        printf("ERROR: NVM not initialized\n");
        return;
    }

    printf("\n========================================\n");
    printf("NVM Data Dump\n");
    printf("========================================\n");

    printf("\nHeader:\n");
    printf("  Magic:    0x%08X %s\n", nvm_cache.magic,
           nvm_cache.magic == NVM_MAGIC_NUMBER ? "(valid)" : "(INVALID!)");
    printf("  Version:  %d\n", nvm_cache.version);
    printf("  CRC32:    0x%08X\n", nvm_cache.crc32);

    printf("\nManufacturing:\n");
    printf("  Date:     %04u-%02u-%02u\n",
           nvm_cache.manufacturing.year,
           nvm_cache.manufacturing.month,
           nvm_cache.manufacturing.day);
    printf("  Serial:   %s\n", nvm_cache.manufacturing.device_serial);
    printf("  Board:    %s\n", nvm_cache.manufacturing.board_revision);
    printf("  HW Ver:   %s\n", nvm_cache.manufacturing.hardware_version);
    printf("  FW Ver:   %s\n", nvm_cache.manufacturing.firmware_version);

    printf("\nCalibration:\n");
    printf("  R_ref values (ohms):\n");
    for (int i = 0; i < 7; i++) {
        printf("    Ch%d: %.2f\n", i, nvm_cache.calibration.r_ref_calibrated[i]);
    }

    printf("\n  RTD Offsets (ohms):\n");
    for (int i = 0; i < 7; i++) {
        printf("    Ch%d: %.4f\n", i, nvm_cache.calibration.rtd_offset[i]);
    }

    printf("\n  RTD Scale factors:\n");
    for (int i = 0; i < 7; i++) {
        printf("    Ch%d: %.6f\n", i, nvm_cache.calibration.rtd_scale[i]);
    }

    printf("\n  ADC Calibration:\n");
    printf("    Offset: %.2f\n", nvm_cache.calibration.adc_offset);
    printf("    Gain:   %.6f\n", nvm_cache.calibration.adc_gain);

    printf("\n  Temperature:\n");
    printf("    Coefficient: %.8f\n", nvm_cache.calibration.temp_coeff);
    printf("    Cal Date:    %u\n", nvm_cache.calibration.calibration_date);

    printf("\n========================================\n");
}

/**
 * Set device serial number
 * Usage: nvm_set_sn <serial>
 */
bool nvm_cmd_set_serial(const char *serial) {
    if (serial == NULL || strlen(serial) == 0) {
        printf("Usage: nvm_set_sn <serial>\n");
        return false;
    }

    if (strlen(serial) >= NVM_MAX_SERIAL_LEN) {
        printf("ERROR: Serial too long (max %d chars)\n", NVM_MAX_SERIAL_LEN - 1);
        return false;
    }

    manufacturing_info_t mfg;
    nvm_read_manufacturing(&mfg);
    strncpy(mfg.device_serial, serial, NVM_MAX_SERIAL_LEN - 1);
    mfg.device_serial[NVM_MAX_SERIAL_LEN - 1] = '\0';

    if (nvm_write_manufacturing(&mfg)) {
        printf("Serial number set to: %s\n", serial);
        return true;
    } else {
        printf("ERROR: Failed to write serial number\n");
        return false;
    }
}

/**
 * Set manufacturing date
 * Usage: nvm_set_date <YYYY-MM-DD>
 */
bool nvm_cmd_set_date(const char *date_str) {
    if (date_str == NULL) {
        printf("Usage: nvm_set_date <YYYY-MM-DD>\n");
        return false;
    }

    uint32_t year, month, day;
    if (sscanf(date_str, "%u-%u-%u", &year, &month, &day) != 3) {
        printf("ERROR: Invalid date format. Use YYYY-MM-DD\n");
        return false;
    }

    if (year < 2020 || year > 2100 || month < 1 || month > 12 || day < 1 || day > 31) {
        printf("ERROR: Invalid date values\n");
        return false;
    }

    manufacturing_info_t mfg;
    nvm_read_manufacturing(&mfg);
    mfg.year = year;
    mfg.month = (uint8_t)month;
    mfg.day = (uint8_t)day;

    if (nvm_write_manufacturing(&mfg)) {
        printf("Manufacturing date set to: %04u-%02u-%02u\n", year, month, day);
        return true;
    } else {
        printf("ERROR: Failed to write manufacturing date\n");
        return false;
    }
}

/**
 * Set board revision
 * Usage: nvm_set_board <revision>
 */
bool nvm_cmd_set_board(const char *board) {
    if (board == NULL || strlen(board) == 0) {
        printf("Usage: nvm_set_board <revision>\n");
        return false;
    }

    if (strlen(board) >= NVM_MAX_SERIAL_LEN) {
        printf("ERROR: Board revision too long (max %d chars)\n", NVM_MAX_SERIAL_LEN - 1);
        return false;
    }

    manufacturing_info_t mfg;
    nvm_read_manufacturing(&mfg);
    strncpy(mfg.board_revision, board, NVM_MAX_SERIAL_LEN - 1);
    mfg.board_revision[NVM_MAX_SERIAL_LEN - 1] = '\0';

    if (nvm_write_manufacturing(&mfg)) {
        printf("Board revision set to: %s\n", board);
        return true;
    } else {
        printf("ERROR: Failed to write board revision\n");
        return false;
    }
}

/**
 * Factory reset command
 * Usage: nvm_reset
 */
bool nvm_cmd_reset(void) {
    printf("Performing factory reset...\n");
    if (nvm_factory_reset()) {
        printf("Factory reset complete\n");
        return true;
    } else {
        printf("ERROR: Factory reset failed\n");
        return false;
    }
}

/**
 * Set calibration R_ref value for a channel
 * Usage: nvm_cal_rref <channel> <value>
 */
bool nvm_cmd_set_rref(int channel, float value) {
    if (channel < 0 || channel >= 7) {
        printf("ERROR: Channel must be 0-6\n");
        return false;
    }

    if (value < 1000.0f || value > 10000.0f) {
        printf("ERROR: R_ref value out of range (1000-10000 ohms)\n");
        return false;
    }

    calibration_info_t cal;
    nvm_read_calibration(&cal);
    cal.r_ref_calibrated[channel] = value;

    if (nvm_write_calibration(&cal)) {
        printf("R_ref[%d] set to: %.2f ohms\n", channel, value);
        return true;
    } else {
        printf("ERROR: Failed to write calibration\n");
        return false;
    }
}

/**
 * Set calibration offset for a channel
 * Usage: nvm_cal_offset <channel> <value>
 */
bool nvm_cmd_set_offset(int channel, float value) {
    if (channel < 0 || channel >= 7) {
        printf("ERROR: Channel must be 0-6\n");
        return false;
    }

    calibration_info_t cal;
    nvm_read_calibration(&cal);
    cal.rtd_offset[channel] = value;

    if (nvm_write_calibration(&cal)) {
        printf("Offset[%d] set to: %.4f ohms\n", channel, value);
        return true;
    } else {
        printf("ERROR: Failed to write calibration\n");
        return false;
    }
}

/**
 * Set calibration scale for a channel
 * Usage: nvm_cal_scale <channel> <value>
 */
bool nvm_cmd_set_scale(int channel, float value) {
    if (channel < 0 || channel >= 7) {
        printf("ERROR: Channel must be 0-6\n");
        return false;
    }

    if (value < 0.5f || value > 2.0f) {
        printf("ERROR: Scale value out of range (0.5-2.0)\n");
        return false;
    }

    calibration_info_t cal;
    nvm_read_calibration(&cal);
    cal.rtd_scale[channel] = value;

    if (nvm_write_calibration(&cal)) {
        printf("Scale[%d] set to: %.6f\n", channel, value);
        return true;
    } else {
        printf("ERROR: Failed to write calibration\n");
        return false;
    }
}

/**
 * Set hardware version
 * Usage: nvm_set_hw_ver <version>
 */
bool nvm_cmd_set_hw_version(const char *version) {
    if (version == NULL || strlen(version) == 0) {
        printf("Usage: nvm_set_hw_ver <version>\n");
        return false;
    }

    if (strlen(version) >= NVM_MAX_SERIAL_LEN) {
        printf("ERROR: Hardware version too long (max %d chars)\n", NVM_MAX_SERIAL_LEN - 1);
        return false;
    }

    manufacturing_info_t mfg;
    nvm_read_manufacturing(&mfg);
    strncpy(mfg.hardware_version, version, NVM_MAX_SERIAL_LEN - 1);
    mfg.hardware_version[NVM_MAX_SERIAL_LEN - 1] = '\0';

    if (nvm_write_manufacturing(&mfg)) {
        printf("Hardware version set to: %s\n", version);
        return true;
    } else {
        printf("ERROR: Failed to write hardware version\n");
        return false;
    }
}

/**
 * Set firmware version
 * Usage: nvm_set_fw_ver <version>
 */
bool nvm_cmd_set_fw_version(const char *version) {
    if (version == NULL || strlen(version) == 0) {
        printf("Usage: nvm_set_fw_ver <version>\n");
        return false;
    }

    if (strlen(version) >= NVM_MAX_SERIAL_LEN) {
        printf("ERROR: Firmware version too long (max %d chars)\n", NVM_MAX_SERIAL_LEN - 1);
        return false;
    }

    manufacturing_info_t mfg;
    nvm_read_manufacturing(&mfg);
    strncpy(mfg.firmware_version, version, NVM_MAX_SERIAL_LEN - 1);
    mfg.firmware_version[NVM_MAX_SERIAL_LEN - 1] = '\0';

    if (nvm_write_manufacturing(&mfg)) {
        printf("Firmware version set to: %s\n", version);
        return true;
    } else {
        printf("ERROR: Failed to write firmware version\n");
        return false;
    }
}
