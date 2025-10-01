#include "deviceID.h"

#if defined(ENSEMBLE_SOC_GEN2)
#define     SOC_ID 2
#elif defined(ENSEMBLE_SOC_E1C)
#define     SOC_ID 1
#else
#define     SOC_ID 0
#endif

uint32_t DeviceID()
{
    return SOC_ID;
}