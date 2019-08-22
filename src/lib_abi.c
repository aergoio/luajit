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
#define INTERNAL_VIEW_FNS       "_view_fns"
#define ABI_CHECK_FEE_DELEGATION "check_delegation"

#define ABI_PROTO_FLAG_NONE     0x00
#define ABI_PROTO_FLAG_PAYABLE  0x01
#define ABI_PROTO_FLAG_VIEW     0x02
#define ABI_PROTO_FLAG_FEEDELEGATION  0x04

#define TYPE_NAME "_type_"
#define TYPE_LEN "_len_"
#define TYPE_DIMENSION "_dimension_"


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
      int newflag; \
      cls = lj_debug_slotname(parent, ip, ra + i + LJ_FR2, &varname); \
      if (NULL == cls || 0 != strcmp(cls, "global")) { \
        luaL_typerror((L), i, "global function"); \
      } \
      if (!lua_isfunction((L),i)) { \
        luaL_typerror((L), i, "global function"); \
      } \
      if (!isluafunc(funcV(L->base+i-1))) { \
        luaL_error(L, LUA_QS " is not a lua function", varname); \
      } \
      lua_pushvalue((L), i); /* payable apis proto */ \
      lua_setfield((L), -2, varname); /* payable apis */ \
      lua_getfield((L), -2, varname); /* payable apis oldflag */ \
      newflag = (flag)|lua_tointeger(L, -1); \
      if (newflag & ABI_PROTO_FLAG_VIEW) { \
      	if ((newflag & ABI_PROTO_FLAG_PAYABLE)) \
        	luaL_error((L), "cannot payable for view function"); \
		change_view_function((L), varname); \
      } \
      if (strcmp(varname, ABI_CHECK_FEE_DELEGATION) == 0) { \
		 if ((newflag & ABI_PROTO_FLAG_VIEW) == 0)  \
        	luaL_error((L), "fee delegation check function must be view function"); \
      } \
      lua_pushinteger((L), newflag); /* payable apis oldflag flag */ \
      lua_setfield((L), -4, varname); /* payable apis oldflag */ \
      lua_pop((L), 1); /* payable apis */ \
    } \
  } while(0)

static int lj_cf_view_call(lua_State *L);

void (*lj_internal_view_start)(lua_State *) = NULL;
void (*lj_internal_view_end)(lua_State *) = NULL;

static void change_view_function(lua_State *L, const char *fname) 
{
  if (luaL_findtable(L, LUA_ENVIRONINDEX, INTERNAL_VIEW_FNS, 5) != NULL) { /* payable apis oldflag ifns*/ 
      luaL_error((L), "cannot load the abi module"); 
  }
  lua_getfield(L, -3, fname); /* payable apis oldflag ifns proto */ 
  lua_setfield(L, -2, fname); /* payable apis oldflag ifns */
  lua_pop(L, 1); /* payable apis oldflag */
  lua_pushstring(L, fname);
  lua_pushcclosure(L, lj_cf_view_call, 1);/* payable apis oldflag cfun*/
  lua_setfield(L, LUA_GLOBALSINDEX, fname); /* payable apis oldflag */
}

static int lj_cf_abi_register(lua_State *L)
{
  REGISTER_EXPORTED_FUNCTION(L, ABI_PROTO_FLAG_NONE);
  return 0;
}

static int lj_cf_abi_register_view(lua_State *L)
{
  REGISTER_EXPORTED_FUNCTION(L, ABI_PROTO_FLAG_VIEW);
  return 0;
}

static int lj_cf_abi_register_fee_delegation(lua_State *L)
{
  REGISTER_EXPORTED_FUNCTION(L, ABI_PROTO_FLAG_FEEDELEGATION);
  return 0;
}

static int lj_cf_abi_register_var(lua_State *L)
{
  luaL_checkstring(L, 1);
  if (!lua_istable(L, 2)) {
    luaL_error(L, "cannot load the abi module");
  }
  if (luaL_findtable(L, LUA_ENVIRONINDEX, ABI_ENV_STATE_VARS, 0) != NULL) {
    luaL_error(L, "cannot load the abi module");
  }
  lua_pushvalue(L, 1);  /* name VT t name */
  lua_rawget(L, -2);    /* name VT t table(or nil) */
  if (!lua_isnil(L, -1)) {
    luaL_error(L, "duplicated variable: " LUA_QS, lua_tostring(L, 1));
  }
  lua_pop(L, 1);        /* name VT t */
  lua_pushvalue(L, 1);  /* name VT t name */
  lua_pushvalue(L, 2);  /* name VT t name VT */
  lua_rawset(L, -3);    /* name VT t */
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
  int flags;
  const char *fname;
  fname = luaL_checkstring(L, 1);
  lua_getfield(L, LUA_ENVIRONINDEX, ABI_ENV_APIS);
  if (!lua_istable(L, -1)) {
    lua_pushnil(L);
    lua_pushnil(L);
    lua_pushnil(L);
    return 3;
  }
  lua_getfield(L, -1, fname);
  if (!lua_isfunction(L, -1)) {
      lua_getfield(L, -2, "default");
      if (!lua_isfunction(L, -1)) {
        lua_pushnil(L);
        lua_pushnil(L);
        lua_pushnil(L);
        return 3;
      }
      fname = "default";
  }
  lua_getfield(L, LUA_ENVIRONINDEX, ABI_ENV_FLAGS);
  if (!lua_istable(L, -1)) {
    lua_pushstring(L, fname);
    lua_pushinteger(L, 0);
    lua_pushinteger(L, 0);
    return 3;
  }

  lua_getfield(L, -1, fname);
  flags = lua_tointeger(L, -1);
  lua_pushstring(L, fname);
  if ((flags & ABI_PROTO_FLAG_PAYABLE) != 0) {
      lua_pushinteger(L, 1);
  } else {
      lua_pushinteger(L, 0);
  }
  if ((flags & ABI_PROTO_FLAG_VIEW) != 0) {
      lua_pushinteger(L, 1);
  } else {
      lua_pushinteger(L, 0);
  }
  return 3;
}

static void autoload_function(lua_State *L, const char *fname, int flags)
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
    lua_pushinteger(L, flags);    /* flags abis flag */
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
  autoload_function(L, "constructor", ABI_PROTO_FLAG_NONE);
  autoload_function(L, "default", ABI_PROTO_FLAG_NONE);
  autoload_function(L, ABI_CHECK_FEE_DELEGATION, ABI_PROTO_FLAG_VIEW);
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
  char has_check_delegation = 0;
  char has_delegation = 0;
  const char *name;
  const char *type;
  const char *dim_lens;
  int len;

  lua_getfield(L, LUA_ENVIRONINDEX, ABI_ENV_FLAGS);
  lua_getfield(L, LUA_ENVIRONINDEX, ABI_ENV_APIS);
  if (!lua_istable(L, -1)) {
    luaL_error(L, "no exported functions. you must add global function(s) to the " LUA_QL("abi.register()"));
  }
  lua_pushliteral(L, "{\"version\":\"0.2\",\"language\":\"lua\",\"functions\":[");
  lua_pushnil(L);
  while (lua_next(L, -3)) {
    if (!lua_isstring(L, -2) || !lua_isfunction(L, -1)) {
      luaL_error(L, "global function expected, got %s, check argument(s) in the " LUA_QL("abi.register()"), 
                 luaL_typename(L, -1));
    }
    name = lua_tostring(L, -2);       /* key is function name */
    if (!isluafunc(funcV(L->top-1))) {
      luaL_error(L, LUA_QS " is not a lua function", name);
    }
    pt = funcproto(funcV(L->top-1));  /* value is function prototype */
    lua_getfield(L, -5, name);
    flag = lua_tointeger(L, -1);
    lua_pop(L, 1);
    if (has_api == 1) {
      CONCAT(lua_pushliteral(L, "},"));
    }
    has_api = 1;
    CONCAT(lua_pushfstring(L, "{\"name\":\"%s\",", name));
	if (strcmp(name, ABI_CHECK_FEE_DELEGATION) == 0) {
		has_check_delegation = 1;
	}
    if (flag & ABI_PROTO_FLAG_VIEW) {
        CONCAT(lua_pushfstring(L, "\"view\":true,"));
    }
    if (flag & ABI_PROTO_FLAG_PAYABLE) {
        CONCAT(lua_pushfstring(L, "\"payable\":true,"));
    }
    if (flag & ABI_PROTO_FLAG_FEEDELEGATION) {
        CONCAT(lua_pushfstring(L, "\"fee_delegation\":true,"));
		has_delegation = 1;
    }
    CONCAT(lua_pushfstring(L, "\"arguments\":["));
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
  if (has_delegation && has_check_delegation == 0) {
    luaL_error(L, "no " LUA_QL(ABI_CHECK_FEE_DELEGATION) " function. you must add global function(s) to the " LUA_QL(ABI_CHECK_FEE_DELEGATION) " with fee_delegation");
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
  lua_pushnil(L);
  while (lua_next(L, -3)) {                         /* t str key val */
    if (!lua_isstring(L, -2) || !lua_istable(L, -1)) {
      luaL_error(L, "invalid state variable");
    }
    if (has_api == 1) {
      CONCAT(lua_pushliteral(L, ","));
    }
    has_api = 1;
    name = lua_tostring(L, -2);
    lua_pushstring(L, TYPE_NAME);                 /* t str key val _type_ */
    lua_rawget(L, -2);                            /* t str key val "type_name" */
    if (!lua_isstring(L, -1)) {
      luaL_error(L, "invalid state variable(" LUA_QS "): unknown type(" LUA_QS ")", name, lua_typename(L, lua_type(L, -1)));
    }
    type = lua_tostring(L, -1);
    if (strcmp(type, "array") == 0) {
      lua_pushstring(L, TYPE_LEN);     /* t str key val "type_name" _len_ */
      lua_rawget(L, -3);			     /* t str key val "type_name" len */
      if (!luaL_isinteger(L, -1)) {
        luaL_error(L, "invalid state variable(" LUA_QS "): invalid field " LUA_QL(TYPE_LEN), name);
      }
      len = lua_tointeger(L, -1);
      if (len < 0) {
        luaL_error(L, "invalid state variable(" LUA_QS "): invalid length(%d)", name, len);
      }
      lua_pop(L, 1);
      lua_getfield(L, -2, TYPE_DIMENSION); /* t str key val "type_name" dim */
      dim_lens = lua_tostring(L, -1);

      lua_pop(L, 2);					/* t str key val */
      if (dim_lens == NULL) {
        CONCAT(lua_pushfstring(L, "{\"name\":\"%s\",\"type\":\"%s\",\"len\":%d}", name, type, len)); /* t new_str key val*/
	  } else {
        CONCAT(lua_pushfstring(L, "{\"name\":\"%s\",\"type\":\"%s\",\"len\":0,\"dimension\":[%s]}", name, type, dim_lens)); /* t new_str key val*/
	  }
    } else if (strcmp(type, "map") == 0) {
	  lua_getfield(L, -2, TYPE_DIMENSION); /* t str key val "type_name" dim */
	  if (luaL_isinteger(L, -1)) {
		int dim = lua_tointeger(L, -1);
        lua_pop(L, 2);  /* t str key val */
		if (dim > 1) 
			CONCAT(lua_pushfstring(L, "{\"name\":\"%s\",\"type\":\"%s\",\"dimension\":%d}", name, type, dim)); /* t new_str key val */
		else
        	CONCAT(lua_pushfstring(L, "{\"name\":\"%s\",\"type\":\"%s\"}", name, type)); /* t new_str key val */
      } else {
        lua_pop(L, 2);  /* t str key val */
        CONCAT(lua_pushfstring(L, "{\"name\":\"%s\",\"type\":\"%s\"}", name, type)); /* t new_str key val */
	  }
	} else if (strcmp(type, "value") == 0) {
      lua_pop(L, 1);  /* t str key val */
      CONCAT(lua_pushfstring(L, "{\"name\":\"%s\",\"type\":\"%s\"}", name, type)); /* t new_str key val */
	} else {
        luaL_error(L, "invalid state variable(" LUA_QS "): unknown type(" LUA_QS ")", name, type);
    }
    lua_pop(L, 1);  /* remove the value*/
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

static int lj_cf_view_call(lua_State *L)
{
  int argc = lua_gettop(L);
  const char *fname;
  int i, status;

  fname = lua_tostring(L, lua_upvalueindex(1));

  if (fname == NULL) {
    luaL_error(L, "cannot find function");
  }
  if (luaL_findtable(L, LUA_ENVIRONINDEX, INTERNAL_VIEW_FNS, argc) != NULL) {
      luaL_error((L), "cannot load the abi module"); 
  } 
  lua_getfield(L, -1, fname);
  if (!lua_isfunction(L, -1)) {
    luaL_error(L, "undefined function: %s", fname);
  }
  if (lj_internal_view_start != NULL)
    lj_internal_view_start(L);
  for (i = 1; i <= argc; i++) {
    lua_pushvalue(L, i);
  }
  status = lua_pcall(L, argc, LUA_MULTRET, 0);
  if (lj_internal_view_end != NULL)
    lj_internal_view_end(L);
  if (status != 0) {
    lua_error(L);
  }
  return lua_gettop(L) - argc -1;
}

static const luaL_Reg abi_lib[] = {
  {"register", lj_cf_abi_register},
  {"register_view", lj_cf_abi_register_view},
  {"register_var", lj_cf_abi_register_var},
  {"fee_delegation", lj_cf_abi_register_fee_delegation},
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
