#include "lj_obj.h"
#include "lj_err.h"
#include "lj_dispatch.h"

LUA_API void lua_gasuse(lua_State *L, int sz)
{        
  if (L2GG(L)->total_gas == 0)
    return;
  if (L2GG(L)->total_gas <= sz) {
    lj_err_gas(L);
  }
  L2GG(L)->total_gas -= sz;
}

LUA_API void lua_gasset(lua_State *L, unsigned long long sz)
{
  L2GG(L)->total_gas = sz;
}

LUA_API unsigned long long lua_gasget(lua_State *L)
{
  return L2GG(L)->total_gas;
}

int lj_gas_strunit(int sz)
{
  return (sz + 9) / 10;
}
