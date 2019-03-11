#include "lj_obj.h"
#include "lj_err.h"
#include "lj_dispatch.h"

void lj_gas_use(lua_State *L, uint64_t sz)
{        
  if (L2GG(L)->total_gas < sz) {
    lj_err_gas(L);
  }
  L2GG(L)->total_gas -= sz;
}
