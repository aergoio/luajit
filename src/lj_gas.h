#ifndef _LJ_GAS_H
#define _LJ_GAS_H

#include "lj_def.h"

#define GAS_ZERO        0
#define GAS_FASTEST     1
#define GAS_FAST        2
#define GAS_MID         3
#define GAS_SLOW        5
#define GAS_EXT        10
#define GAS_MEM         3

void lj_gas_use(lua_State *L, uint64_t sz);

#endif /* _LJ_GAS_H */
