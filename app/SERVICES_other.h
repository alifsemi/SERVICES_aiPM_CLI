#include <stdint.h>

#if defined(ENSEMBLE_SOC_E1C)
#define EWIC_VBAT_TIMER0            0x020       // bit5
#define EWIC_VBAT_TIMER1            0x040       // bit6
#define EWIC_VBAT_GPIO0             0x200       // bit9
#define EWIC_VBAT_GPIO1             0x400       // bit10
#else
#define EWIC_VBAT_TIMER0            0x080       // bit7
#define EWIC_VBAT_TIMER1            0x100       // bit8
#define EWIC_VBAT_TIMER2            0x200       // bit9
#define EWIC_VBAT_TIMER3            0x400       // bit10
#define EWIC_VBAT_GPIO0             0x00800     // bit11
#define EWIC_VBAT_GPIO1             0x01000     // bit12
#define EWIC_VBAT_GPIO2             0x02000     // bit13
#define EWIC_VBAT_GPIO3             0x04000     // bit14
#define EWIC_VBAT_GPIO4             0x08000     // bit15
#define EWIC_VBAT_GPIO5             0x10000     // bit16
#define EWIC_VBAT_GPIO6             0x20000     // bit17
#define EWIC_VBAT_GPIO7             0x40000     // bit18
#endif

void print_se_startup_info();
void SERVICES_boot_testing(int32_t boot_selection, int32_t cpu_selection);
void SERVICES_ret(uint32_t ret);
void SERVICES_response(uint32_t response);
