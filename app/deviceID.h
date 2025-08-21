#include <stdint.h>

/* DeviceID, used to identify differences between silicon revisions
 *  0   - Bolt Silicon Rev. Bx (Ensemble E1, E3, E5, E7)
 *  1   - Spark Silicon Rev. Ax (Balletto B1C, Ensemble E1C)
 *  2   - Eagle Silicon Rev. Ax (Ensemble E4, E6, E8)
 */
uint32_t DeviceID();
