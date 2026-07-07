#include "calibration_storage.h"
#include "secure_nsc.h"
#include <string.h>

void CalibrationStorage_Init(void)
{
    /* EEPROM emulation is initialized by Secure firmware (EE_Init).
     * No initialization required in NonSecure world. */
}

uint32_t CalibrationStorage_WriteFloat(uint16_t vaddr, float value)
{
    uint32_t bits = 0;
    memcpy(&bits, &value, sizeof(float));
    return SECURE_CAL_Write(vaddr, bits);
}

uint32_t CalibrationStorage_ReadFloat(uint16_t vaddr, float *pValue)
{
    if (pValue == NULL)
    {
        return 1U;
    }

    uint32_t bits = 0;
    uint32_t status = SECURE_CAL_Read(vaddr, &bits);
    if (status == 0)
    {
        memcpy(pValue, &bits, sizeof(float));
    }
    return status;
}

uint32_t CalibrationStorage_WriteU32(uint16_t vaddr, uint32_t value)
{
    return SECURE_CAL_Write(vaddr, value);
}

uint32_t CalibrationStorage_ReadU32(uint16_t vaddr, uint32_t *pValue)
{
    return SECURE_CAL_Read(vaddr, pValue);
}
