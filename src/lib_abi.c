#define lib_abi_c
#define LUA_LIB

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "lj_bc.h"
#include "lj_debug.h"
#include "lj_arch.h"

#define ABI_ENV_APIS            "apis"
#define ABI_ENV_FLAGS           "flags"
#define ABI_ENV_STATE_VARS      "state_vars"

#define ABI_PROTO_FLAG_NONE     0x00
#define ABI_PROTO_FLAG_PAYABLE  0x01

#define REGISTER_EXPORTED_FUNCTION(L, flag) \
  do { \
    int argc; \
    const BCIns *ip; \
    GCproto *parent = NULL; \
    int i; \
    int ra; \
    argc = lua_gettop((L)); \
    if (argc < 1) { \
      return 0; \
    } \
    ip = lj_debug_callip((L), &parent); \
    if (ip == NULL || (int)bc_c(*ip) - 1 != argc) { \
      luaL_error((L), "cannot load the abi module"); \
    } \
    if (luaL_findtable((L), LUA_ENVIRONINDEX, ABI_ENV_FLAGS, argc) != NULL) { \
      luaL_error((L), "cannot load the abi module"); \
    } \
    if (luaL_findtable((L), LUA_ENVIRONINDEX, ABI_ENV_APIS, argc) != NULL) { \
      luaL_error((L), "cannot load the abi module"); \
    } \
    /* payable apis */ \
    ra = bc_a(*ip); \
    for (i = 1; i <= argc; i++) { \
      const char *cls, *varname; \
      cls = lj_debug_slotname(parent, ip, ra + i + LJ_FR2, &varname); \
      if (NULL == cls || 0 != strcmp(cls, "global")) { \
        luaL_typerror((L), i, "global function"); \
      } \
      if (!lua_isfunction((L),i)) { \
        luaL_typerror((L), i, "global function"); \
      } \
      lua_pushvalue((L), i); /* payable apis proto */ \
      lua_setfield((L), -2, varname); /* payable apis */ \
      lua_getfield((L), -2, varname); /* payable apis oldflag */ \
      lua_pushinteger((L), (flag)|lua_tointeger(L, -1)); /* payable apis oldflag flag */ \
      lua_setfield((L), -4, varname); /* payable apis oldflag */ \
      lua_pop((L), 1); /* payable apis */ \
    } \
  } while(0)

static int lj_cf_abi_register(lua_State *L)
{
  REGISTER_EXPORTED_FUNCTION(L, ABI_PROTO_FLAG_NONE);
  return 0;
}

static int lj_cf_abi_register_var(lua_State *L)
{
  luaL_checkstring(L, 1);
  luaL_checkstring(L, 2);
  if (luaL_findtable(L, LUA_ENVIRONINDEX, ABI_ENV_STATE_VARS, 0) != NULL) {
    luaL_error(L, "cannot load the abi module");
  }
  lua_pushvalue(L, 1);  /* name type t name */
  lua_rawget(L, -2);    /* name type t type(or nil) */
  if (!lua_isnil(L, -1)) {
    luaL_error(L, "duplicated variable: " LUA_QL("%s"), lua_tostring(L, 1));
  }
  lua_pop(L, 1);        /* name type t */
  lua_pushvalue(L, 1);  /* name type t name */
  lua_pushvalue(L, 2);  /* name type t name type */
  lua_rawset(L, -3);    /* name type t */
  return 0;
}

static int lj_cf_abi_register_payable(lua_State *L)
{
  REGISTER_EXPORTED_FUNCTION(L, ABI_PROTO_FLAG_PAYABLE);
  return 0;
}

static int lj_cf_abi_is_payable(lua_State *L)
{
  const char *fname;
  int flag;
  fname = luaL_checkstring(L, 1);
  lua_getfield(L, LUA_ENVIRONINDEX, ABI_ENV_FLAGS);
  if (!lua_istable(L, -1)) {
    lua_pushinteger(L, 0);
    return 1;

  }
  lua_getfield(L, -1, fname);
  flag = lua_tointeger(L, -1);
  if ((flag & ABI_PROTO_FLAG_PAYABLE) != 0) {
      lua_pushinteger(L, 1);
  } else {
      lua_pushinteger(L, 0);
  }
  return 1;
}

static int lj_cf_abi_resolve(lua_State *L)
{
  const char *fname;
  fname = luaL_checkstring(L, 1);
  lua_getfield(L, LUA_ENVIRONINDEX, ABI_ENV_APIS);
  if (!lua_istable(L, -1)) {
    lua_pushnil(L);
    return 1;

  }
  lua_getfield(L, -1, fname);
  if (lua_isfunction(L, -1)) {
    lua_pushvalue(L, 1);
    return 1;
  }
  lua_getfield(L, -2, "default");
  if (lua_isfunction(L, -1)) {
    lua_pushstring(L, "default");
    return 1;
  } else {
    return 0;
  }
}

static void autoload_function(lua_State *L, const char *fname)
{
  /* flags abis */
  lua_getfield(L, -1, fname);                   /* flags abis proto */
  if (!lua_isnil(L, -1)) {
    lua_pop(L, 1);
    return;
  }
  lua_pop(L, 1);
  lua_getfield(L, LUA_GLOBALSINDEX, fname);     /* flags abis proto */
  if (lua_isfunction(L, -1)) {
    lua_setfield(L, -2, fname);                 /* flags abis */
    lua_pushinteger(L, ABI_PROTO_FLAG_NONE);    /* flags abis flag */
    lua_setfield(L, -3, fname);                 /* flags abis */
  } else {
    lua_pop(L, 1);
  }
}

static int lj_cf_abi_autoload(lua_State *L)
{
  if (luaL_findtable((L), LUA_ENVIRONINDEX, ABI_ENV_FLAGS, 2) != NULL) {
    luaL_error((L), "cannot load the abi module");
  }
  if (luaL_findtable((L), LUA_ENVIRONINDEX, ABI_ENV_APIS, 2) != NULL) {
    luaL_error((L), "cannot load the abi module");
  }
  autoload_function(L, "constructor");
  autoload_function(L, "default");
  lua_pop(L, 2);
  return 0;
}

#define CONCAT(f) \
  do { \
    lua_pushvalue(L, -3); \
    (f); \
    lua_concat(L, 2); \
    lua_replace(L, -4); \
  } while(0)

static int lj_cf_abi_generate(lua_State *L)
{
  GCproto *pt;
  int i, flag;
  int has_api = 0;
  const char *name;
  const char *type;

  lua_getfield(L, LUA_ENVIRONINDEX, ABI_ENV_FLAGS);
  lua_getfield(L, LUA_ENVIRONINDEX, ABI_ENV_APIS);
  if (!lua_istable(L, -1)) {
    luaL_error(L, "no exported functions. you must add global function(s) to the " LUA_QL("abi.register()"));
  }
  lua_pushliteral(L, "{\"version\":\"0.2\",\"language\":\"lua\",\"functions\":[");
  setnilV(L->top++);
  while (lua_next(L, -3)) {
    if (!lua_isstring(L, -2) || !lua_isfunction(L, -1)) {
      luaL_error(L, "global function expected, got %s, check argument(s) in the " LUA_QL("abi.register()"), 
                 luaL_typename(L, -1));
    }
    name = strdata(strV(L->top-2));   /* key is function name */
    pt = funcproto(funcV(L->top-1));  /* value is function prototype */
    lua_getfield(L, -5, name);
    flag = lua_tointeger(L, -1);
    lua_pop(L, 1);
    if (has_api == 1) {
      CONCAT(lua_pushliteral(L, "},"));
    }
    has_api = 1;
    if (flag & ABI_PROTO_FLAG_PAYABLE) {
        CONCAT(lua_pushfstring(L, "{\"name\":\"%s\",\"payable\":true,\"arguments\":[", name));
    } else {
        CONCAT(lua_pushfstring(L, "{\"name\":\"%s\",\"arguments\":[", name));
    }
    for (i = 1; i <= pt->numparams; i++) {
      name = lua_getlocal(L, NULL, i);
      if (i == pt->numparams && !(pt->flags & PROTO_VARARG)) {
        CONCAT(lua_pushfstring(L, "{\"name\":\"%s\"}", name));
      } else {
        CONCAT(lua_pushfstring(L, "{\"name\":\"%s\"},", name));
      }
    }
    if (pt->flags & PROTO_VARARG) {
      CONCAT(lua_pushliteral(L, "{\"name\":\"...\"}"));
    }
    CONCAT(lua_pushfstring(L, "]", name));
    lua_pop(L, 1);  /* remove the table and the proto, the key is reused */
  }
  if (has_api == 0) {
    luaL_error(L, "no exported functions. you must add global function(s) to the " LUA_QL("abi.register()"));
  }
  lua_getfield(L, LUA_ENVIRONINDEX, ABI_ENV_STATE_VARS);
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
    lua_pushliteral(L, "}]}");
    lua_concat(L, 2);
    return 1;
  }
  has_api = 0;
  lua_pushvalue(L, -2);                             /* t(state_vars) str(functions[{) */
  lua_pushliteral(L, "}],\"state_variables\":[");   /* t str str */
  lua_concat(L, 2);                                 /* t str */
  setnilV(L->top++);                                /* t str nil */
  while (lua_next(L, -3)) {                         /* t str key val */
    if (!tvisstr(L->top-2) || !tvisstr(L->top-1)) {
      luaL_error(L, "invalid state variable");
    }
    if (has_api == 1) {
      CONCAT(lua_pushliteral(L, ","));
    }
    has_api = 1;
    name = strdata(strV(L->top-2));
    type = strdata(strV(L->top-1));
    CONCAT(lua_pushfstring(L, "{\"name\":\"%s\",\"type\":\"%s\"}", name, type)); /* t new_str key val */
    lua_pop(L, 1);  /* remove the value */
  }
  lua_pushstring(L, "]}");
  lua_concat(L, 2);
  return 1;
}

static int lj_cf_abi_call(lua_State *L)
{
  int argc = lua_gettop(L);
  const char *fname;
  int i;

  if (argc < 1) {
    return 0;
  }
  lua_getfield(L, LUA_ENVIRONINDEX, ABI_ENV_APIS);
  if (!lua_istable(L, -1)) {
    return 0;
  }
  fname = luaL_checkstring(L, 1);
  lua_getfield(L, -1, fname);
  if (!lua_isfunction(L, -1)) {
    luaL_error(L, "undefined function: %s", fname);
  }
  for (i = 2; i <= argc; i++) {
    lua_pushvalue(L, i);
  }
  lua_call(L, argc - 1, LUA_MULTRET);
  return lua_gettop(L) - argc - 1;
}

static const luaL_Reg abi_lib[] = {
  {"register", lj_cf_abi_register},
  {"register_var", lj_cf_abi_register_var},
  {"payable", lj_cf_abi_register_payable},
  {"is_payable", lj_cf_abi_is_payable},
  {"resolve", lj_cf_abi_resolve},
  {"autoload", lj_cf_abi_autoload},
  {"generate", lj_cf_abi_generate},
  {"call", lj_cf_abi_call},
  {NULL, NULL}
};

int luaopen_abi(lua_State *L)
{
  lua_createtable(L, 0, 2);
  lua_replace(L, LUA_ENVIRONINDEX);
  luaL_register(L, LUA_ABILIBNAME, abi_lib);
  return 1;
}
