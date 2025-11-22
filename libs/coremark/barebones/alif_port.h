#include "stddef.h"
#include "sys_clocks.h"
#define CLOCKS_PER_SEC  32768

/* 
 * to achieve the best results:
 * use Arm Clang with -Omax for ~4.38 Iter./Sec/MHz
 * otherwise Arm Clang with -O3 is ~3.96 Iter./Sec/MHz
 * and GCC with -O3 is ~3.10 Iter./Sec/MHz
 */
#if defined(_DEBUG)
#define FLAGS_STR       "-Og"
#else
#define FLAGS_STR       "-O3"
#endif

#if defined(M55_HE)
#define ITERATIONS      8000
#else
#define ITERATIONS      14000
#endif