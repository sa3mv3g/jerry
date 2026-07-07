#ifndef CALIBRATION_STORAGE_H
#define CALIBRATION_STORAGE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Initialize calibration storage (no-op in NS world, as Secure world
     * initializes EEPROM)
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

#ifdef __cplusplus
}
#endif

#endif /* CALIBRATION_STORAGE_H */
