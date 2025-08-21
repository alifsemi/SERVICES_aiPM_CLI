#include <stdint.h>

#define RTSS_BIN    0
#define MDM_BIN     0

/* resetting the external system leaves it in the off state
 * vtor_address is where the cpu will start after release
 */
void reset_external_system0(uint32_t vtor_address);
void reset_external_system1(uint32_t vtor_address);

void release_external_system0();
void release_external_system1();
void release_m55m();
void release_m55g();

#ifdef RTSS_BIN
void sideload_external_system0();
void sideload_external_system1();
#endif
#ifdef MDM_BIN
void sideload_m55m();
void sideload_m55g();
#endif

