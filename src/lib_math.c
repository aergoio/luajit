/*
** Math library.
** Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
*/

#include <math.h>

#define lib_math_c
#define LUA_LIB

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lj_obj.h"
#include "lj_lib.h"
#include "lj_vm.h"
#include "lj_err.h"

/* ------------------------------------------------------------------------ */

#define LJLIB_MODULE_math

LJLIB_ASM(math_abs)		LJLIB_REC(.)
{
  lj_lib_checknumber(L, 1);
  return FFH_RETRY;
}
LJLIB_ASM_(math_floor)		LJLIB_REC(math_round IRFPM_FLOOR)
LJLIB_ASM_(math_ceil)		LJLIB_REC(math_round IRFPM_CEIL)

LJLIB_ASM(math_pow)		LJLIB_REC(.)
{
  lj_lib_checknum(L, 1);
  lj_lib_checknum(L, 2);
  return FFH_RETRY;
}

LJLIB_ASM(math_min)		LJLIB_REC(math_minmax IR_MIN)
{
  int i = 0;
  do {
      lj_lib_checknumber(L, ++i);
  } while (L->base+i < L->top);
  return FFH_RETRY;
}
LJLIB_ASM_(math_max)		LJLIB_REC(math_minmax IR_MAX)

/* ------------------------------------------------------------------------ */

/* This implements a Tausworthe PRNG with period 2^223. Based on:
**   Tables of maximally-equidistributed combined LFSR generators,
**   Pierre L'Ecuyer, 1991, table 3, 1st entry.
** Full-period ME-CF generator with L=64, J=4, k=223, N1=49.
*/

/* PRNG state. */
struct RandomState {
  uint64_t gen[4];	/* State of the 4 LFSR generators. */
  int valid;		/* State is valid. */
};

/* Union needed for bit-pattern conversion between uint64_t and double. */
typedef union { uint64_t u64; double d; } U64double;

/* Update generator i and compute a running xor of all states. */
#define TW223_GEN(i, k, q, s) \
  z = rs->gen[i]; \
  z = (((z<<q)^z) >> (k-s)) ^ ((z&((uint64_t)(int64_t)-1 << (64-k)))<<s); \
  r ^= z; rs->gen[i] = z;

/* PRNG step function. Returns a double in the range 1.0 <= d < 2.0. */
LJ_NOINLINE uint64_t LJ_FASTCALL lj_math_random_step(RandomState *rs)
{
  uint64_t z, r = 0;
  TW223_GEN(0, 63, 31, 18)
  TW223_GEN(1, 58, 19, 28)
  TW223_GEN(2, 55, 24,  7)
  TW223_GEN(3, 47, 21,  8)
  return (r & U64x(000fffff,ffffffff)) | U64x(3ff00000,00000000);
}

/* PRNG initialization function. */
static void random_init(RandomState *rs, double d)
{
  uint32_t r = 0x11090601;  /* 64-k[i] as four 8 bit constants. */
  int i;
  for (i = 0; i < 4; i++) {
    U64double u;
    uint32_t m = 1u << (r&255);
    r >>= 8;
    u.d = d = d * 3.14159265358979323846 + 2.7182818284590452354;
    if (u.u64 < m) u.u64 += m;  /* Ensure k[i] MSB of gen[i] are non-zero. */
    rs->gen[i] = u.u64;
  }
  rs->valid = 1;
  for (i = 0; i < 10; i++)
    lj_math_random_step(rs);
}

/* PRNG seed function. */
LJLIB_PUSH(top-2)  /* Upvalue holds userdata with RandomState. */
LJLIB_CF(math_randomseed)
{
  lj_err_callermsg(L, LUA_QL("math.randomseed") " not supported");
  RandomState *rs = (RandomState *)(uddata(udataV(lj_lib_upvalue(L, 1))));
  random_init(rs, lj_lib_checknum(L, 1));
  return 0;
}

/* ------------------------------------------------------------------------ */

#include "lj_libdef.h"

LUALIB_API int luaopen_math(lua_State *L)
{
  RandomState *rs;
  rs = (RandomState *)lua_newuserdata(L, sizeof(RandomState));
  rs->valid = 0;  /* Use lazy initialization to save some time on startup. */
  LJ_LIB_REG(L, LUA_MATHLIBNAME, math);
  return 1;
}

