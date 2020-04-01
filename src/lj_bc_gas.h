#ifndef _LJ_BC_GAS_H
#define _LJ_BC_GAS_H

#include "lj_def.h"
#include "lj_bc.h"

uint16_t bc_gas[] = {
#define BC_GAS(name, ma, mb, mc, mt, gas)	gas,
    BCDEF(BC_GAS)
#undef BC_GAS
};

#endif /* _LJ_BC_GAS_H */
