#include "lj_obj.h"
#include "lj_err.h"
#include "lj_dispatch.h"

static unsigned long long lj_gas_mul_no_ovt(unsigned long long a, unsigned long long b)
{
  unsigned long long r;
  if (a == 0 || b == 0) {
      return 0;
  }
  r = a * b;
  if (b > UINT64_MAX / a) {
      return UINT64_MAX;
  }
  return r;
}

LUA_API void lua_gasuse(lua_State *L, unsigned long long sz)
{        
  GG_State *gg = L2GG(L);
  if (gg->use_gas == 0)
    return;
  if (gg->total_gas < sz) {
    lj_err_gas(L);
  }
  gg->total_gas -= sz;
}

LUA_API void lua_gasuse_mul(lua_State *L, unsigned long long sz, unsigned long long n)
{
  lua_gasuse(L, lj_gas_mul_no_ovt(sz, n));
}

LUA_API void lua_gasset(lua_State *L, unsigned long long sz)
{
  L2GG(L)->use_gas = 1;
  L2GG(L)->total_gas = sz;
}

LUA_API unsigned long long lua_gasget(lua_State *L)
{
  return L2GG(L)->total_gas;
}

int32_t lj_gas_strunit(int32_t sz)
{
  return (sz+LJ_GAS_STRUNIT-1) / LJ_GAS_STRUNIT;
}

LUA_API unsigned char lua_setusegas(lua_State *L, unsigned char use)
{
  uint8_t before = L2GG(L)->use_gas;
  L2GG(L)->use_gas = use;
  return (unsigned char)before;
}
