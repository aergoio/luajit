#define lib_abi_c
#define LUA_LIB

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "lj_bc.h"
#include "lj_debug.h"

static int lj_cf_abi_register(lua_State *L)
{
  int argc;
  const BCIns *ip;
  GCproto *parent = NULL;
  int i;
  int ra;
  argc = lua_gettop(L);
  if (argc < 1) {
    return 0;
  }
  ip = lj_debug_callip(L, &parent);
  if (ip == NULL || (int)bc_c(*ip) - 1 != argc) {
    luaL_error(L, "cannot load the abi module");
  }
  if (luaL_findtable(L, LUA_ENVIRONINDEX, "apis", argc) != NULL) {
    luaL_error(L, "cannot load the abi module");
  }
  ra = bc_a(*ip);
  for (i = 1; i <= argc; i++) {
    const char *cls, *varname;
    cls = lj_debug_slotname(parent, ip, ra + i, &varname);
    if (NULL == cls || 0 != strcmp(cls, "global")) {
      luaL_typerror(L, i, "global function");
    }
    if (!lua_isfunction(L,i)) {
      luaL_typerror(L, i, "global function");
    }
    lua_pushstring(L, varname);
    lua_pushvalue(L, i);
    lua_rawset(L, -3);
  }
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
  int i;
  int has_api = 0;
  const char *name;

  lua_getfield(L, LUA_ENVIRONINDEX, "apis");
  if (!lua_istable(L, -1)) {
    return 0;
  }
  lua_pushliteral(L, "{\"version\":\"0.1\",\"language\":\"lua\",\"functions\":[");
  setnilV(L->top++);
  while (lua_next(L, -3)) {
    if (has_api == 1) {
      CONCAT(lua_pushliteral(L, "},"));
    }
    has_api = 1;
    if (!tvisstr(L->top-2) || !tvisfunc(L->top-1)) {
      luaL_error(L, "cannot load the abi module");
    }
    name = strdata(strV(L->top-2));   /* key is function name */
    pt = funcproto(funcV(L->top-1));  /* value is function prototype */
    CONCAT(lua_pushfstring(L, "{\"name\":\"%s\",\"arguments\":[", name));
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
    lua_pop(L, 1);  /* remove a value, the key is reused */
  }
  if (has_api == 0) {
    return 0;
  }
  lua_pushvalue(L, -1);
  lua_pushliteral(L, "}]}");
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
  lua_getfield(L, LUA_ENVIRONINDEX, "apis");
  if (!lua_istable(L, -1)) {
    return 0;
  }
  fname = luaL_checkstring(L, 1);
  lua_getfield(L, -1, fname);
  if (!lua_isfunction(L, -1)) {
    luaL_error(L, "cannot find the function: %s", fname);
  }
  for (i = 2; i <= argc; i++) {
    lua_pushvalue(L, i);
  }
  lua_call(L, argc - 1, LUA_MULTRET);
  return lua_gettop(L) - argc - 1;
}

static const luaL_Reg abi_lib[] = {
  {"register", lj_cf_abi_register},
  {"generate", lj_cf_abi_generate},
  {"call", lj_cf_abi_call},
  {NULL, NULL}
};

int luaopen_abi(lua_State *L)
{
  lua_createtable(L, 0, 1);
  lua_replace(L, LUA_ENVIRONINDEX);
  luaL_register(L, LUA_ABILIBNAME, abi_lib);
  return 1;
}
