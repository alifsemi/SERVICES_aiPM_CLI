#include "stddef.h"
#include "sys_clocks.h"
#define CLOCKS_PER_SEC  32768

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