#ifndef CALIBRATION_STORAGE_H
#define CALIBRATION_STORAGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize calibration storage (no-op in NS world, as Secure world initializes EEPROM)
 */
void CalibrationStorage_Init(void);

/**
 * @brief Write float calibration value to storage
 * @param vaddr Virtual address (from secure_nsc.h CAL_* defines)
 * @param value Float value to write
 * @return 0 on success, non-zero on error
 */
uint32_t CalibrationStorage_WriteFloat(uint16_t vaddr, float value);

/**
 * @brief Read float calibration value from storage
 * @param vaddr Virtual address (from secure_nsc.h CAL_* defines)
 * @param pValue Pointer to store the read float value
 * @return 0 on success, non-zero on error
 */
uint32_t CalibrationStorage_ReadFloat(uint16_t vaddr, float *pValue);

/**
 * @brief Write uint32_t calibration value to storage
 * @param vaddr Virtual address
 * @param value uint32_t value to write
 * @return 0 on success, non-zero on error
 */
uint32_t CalibrationStorage_WriteU32(uint16_t vaddr, uint32_t value);

/**
 * @brief Read uint32_t calibration value from storage
 * @param vaddr Virtual address
 * @param pValue Pointer to store the read uint32_t value
 * @return 0 on success, non-zero on error
 */
uint32_t CalibrationStorage_ReadU32(uint16_t vaddr, uint32_t *pValue);

/**
 * @brief  Check FOTA valid flag on startup and roll back if not set.
 *         Reads FOTA_VALID_FLAG_VADDR from EDATA via SECURE_CAL_Read().
 *         If the flag is not 0xA5A5A5A5, calls SECURE_FOTA_Rollback()
 *         which triggers a system reset — this function does NOT return
 *         in that case. If the flag is set, returns normally.
 */
void fota_startup_check(void);

/**
 * @brief  Mark the current firmware as valid after a successful self-test.
 *         Writes 0xA5A5A5A5 to FOTA_VALID_FLAG_VADDR in EDATA.
 *         Call after the application has verified it is running correctly.
 */
void fota_mark_valid(void);

#ifdef __cplusplus
}
#endif

#endif /* CALIBRATION_STORAGE_H */
