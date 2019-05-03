/*
** Package library.
** Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
**
** Major portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2012 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#define lib_package_c
#define LUA_LIB

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lj_obj.h"
#include "lj_err.h"
#include "lj_lib.h"

/* ------------------------------------------------------------------------ */

#define setprogdir(L)		((void)0)

#if LJ_TARGET_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifndef GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
#define GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS  4
#define GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT  2
BOOL WINAPI GetModuleHandleExA(DWORD, LPCSTR, HMODULE*);
#endif

#if LJ_TARGET_UWP
void *LJ_WIN_LOADLIBA(const char *path)
{
  DWORD err = GetLastError();
  wchar_t wpath[256];
  HANDLE lib = NULL;
  if (MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, 256) > 0) {
    lib = LoadPackagedLibrary(wpath, 0);
  }
  SetLastError(err);
  return lib;
}
#endif

#undef setprogdir

static void setprogdir(lua_State *L)
{
  char buff[MAX_PATH + 1];
  char *lb;
  DWORD nsize = sizeof(buff);
  DWORD n = GetModuleFileNameA(NULL, buff, nsize);
  if (n == 0 || n == nsize || (lb = strrchr(buff, '\\')) == NULL) {
    luaL_error(L, "unable to get ModuleFileName");
  } else {
    *lb = '\0';
    luaL_gsub(L, lua_tostring(L, -1), LUA_EXECDIR, buff);
    lua_remove(L, -2);  /* remove original string */
  }
}

#endif

/* ------------------------------------------------------------------------ */

#if LJ_ENABLE_DEBUG
static int readable(const char *filename)
{
  FILE *f = fopen(filename, "r");  /* try to open file */
  if (f == NULL) return 0;  /* open failed */
  fclose(f);
  return 1;
}

static const char *pushnexttemplate(lua_State *L, const char *path)
{
  const char *l;
  while (*path == *LUA_PATHSEP) path++;  /* skip separators */
  if (*path == '\0') return NULL;  /* no more templates */
  l = strchr(path, *LUA_PATHSEP);  /* find next separator */
  if (l == NULL) l = path + strlen(path);
  lua_pushlstring(L, path, (size_t)(l - path));  /* template */
  return l;
}

static const char *searchpath (lua_State *L, const char *name,
			       const char *path, const char *sep,
			       const char *dirsep)
{
  luaL_Buffer msg;  /* to build error message */
  luaL_buffinit(L, &msg);
  if (*sep != '\0')  /* non-empty separator? */
    name = luaL_gsub(L, name, sep, dirsep);  /* replace it by 'dirsep' */
  while ((path = pushnexttemplate(L, path)) != NULL) {
    const char *filename = luaL_gsub(L, lua_tostring(L, -1),
				     LUA_PATH_MARK, name);
    lua_remove(L, -2);  /* remove path template */
    if (readable(filename))  /* does file exist and is readable? */
      return filename;  /* return that file name */
    lua_pushfstring(L, "\n\tno file " LUA_QS, filename);
    lua_remove(L, -2);  /* remove file name */
    luaL_addvalue(&msg);  /* concatenate error msg. entry */
  }
  luaL_pushresult(&msg);  /* create error message */
  return NULL;  /* not found */
}

static const char *findfile(lua_State *L, const char *name,
			    const char *pname)
{
  const char *path;
  lua_getfield(L, LUA_ENVIRONINDEX, pname);
  path = lua_tostring(L, -1);
  if (path == NULL)
    luaL_error(L, LUA_QL("package.%s") " must be a string", pname);
  return searchpath(L, name, path, ".", LUA_DIRSEP);
}

static void loaderror(lua_State *L, const char *filename)
{
  luaL_error(L, "error loading module " LUA_QS " from file " LUA_QS ":\n\t%s",
	     lua_tostring(L, 1), filename, lua_tostring(L, -1));
}
#endif

static int lj_cf_package_loader_lua(lua_State *L)
{
#if LJ_ENABLE_DEBUG
  const char *filename;
  const char *name = luaL_checkstring(L, 1);
  filename = findfile(L, name, "path");
  if (filename == NULL) return 1;  /* library not found in this path */
  if (luaL_loadfile(L, filename) != 0)
    loaderror(L, filename);
#endif
  return 1;  /* library loaded successfully */
}

static int lj_cf_package_loader_preload(lua_State *L)
{
  const char *name = luaL_checkstring(L, 1);
  lua_getfield(L, LUA_ENVIRONINDEX, "preload");
  if (!lua_istable(L, -1))
    luaL_error(L, LUA_QL("package.preload") " must be a table");
  lua_getfield(L, -1, name);
  if (lua_isnil(L, -1)) {  /* Not found? */
    lua_pushfstring(L, "\n\tno field package.preload['%s']", name);
  }
  return 1;
}

/* ------------------------------------------------------------------------ */

#define sentinel	((void *)0x4004)

static int lj_cf_package_require(lua_State *L)
{
  const char *name = luaL_checkstring(L, 1);
  int i;
  lua_settop(L, 1);  /* _LOADED table will be at index 2 */
  lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
  lua_getfield(L, 2, name);
  if (lua_toboolean(L, -1)) {  /* is it there? */
    if (lua_touserdata(L, -1) == sentinel)  /* check loops */
      luaL_error(L, "loop or previous error loading module " LUA_QS, name);
    return 1;  /* package is already loaded */
  }
  /* else must load it; iterate over available loaders */
  lua_getfield(L, LUA_ENVIRONINDEX, "loaders");
  if (!lua_istable(L, -1))
    luaL_error(L, LUA_QL("package.loaders") " must be a table");
  lua_pushliteral(L, "");  /* error message accumulator */
  for (i = 1; ; i++) {
    lua_rawgeti(L, -2, i);  /* get a loader */
    if (lua_isnil(L, -1))
      luaL_error(L, "module " LUA_QS " not found:%s",
		 name, lua_tostring(L, -2));
    lua_pushstring(L, name);
    lua_call(L, 1, 1);  /* call it */
    if (lua_isfunction(L, -1))  /* did it find module? */
      break;  /* module loaded successfully */
    else if (lua_isstring(L, -1))  /* loader returned error message? */
      lua_concat(L, 2);  /* accumulate it */
    else
      lua_pop(L, 1);
  }
  lua_pushlightuserdata(L, sentinel);
  lua_setfield(L, 2, name);  /* _LOADED[name] = sentinel */
  lua_pushstring(L, name);  /* pass name as argument to module */
  lua_call(L, 1, 1);  /* run loaded module */
  if (!lua_isnil(L, -1))  /* non-nil return? */
    lua_setfield(L, 2, name);  /* _LOADED[name] = returned value */
  lua_getfield(L, 2, name);
  if (lua_touserdata(L, -1) == sentinel) {   /* module did not set a value? */
    lua_pushboolean(L, 1);  /* use true as result */
    lua_pushvalue(L, -1);  /* extra copy to be returned */
    lua_setfield(L, 2, name);  /* _LOADED[name] = true */
  }
  lj_lib_checkfpu(L);
  return 1;
}

/* ------------------------------------------------------------------------ */

#if LJ_ENABLE_DEBUG
static void setfenv(lua_State *L)
{
  lua_Debug ar;
  if (lua_getstack(L, 1, &ar) == 0 ||
      lua_getinfo(L, "f", &ar) == 0 ||  /* get calling function */
      lua_iscfunction(L, -1))
    luaL_error(L, LUA_QL("module") " not called from a Lua function");
  lua_pushvalue(L, -2);
  lua_setfenv(L, -2);
  lua_pop(L, 1);
}

static void dooptions(lua_State *L, int n)
{
  int i;
  for (i = 2; i <= n; i++) {
    lua_pushvalue(L, i);  /* get option (a function) */
    lua_pushvalue(L, -2);  /* module */
    lua_call(L, 1, 0);
  }
}

static void modinit(lua_State *L, const char *modname)
{
  const char *dot;
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "_M");  /* module._M = module */
  lua_pushstring(L, modname);
  lua_setfield(L, -2, "_NAME");
  dot = strrchr(modname, '.');  /* look for last dot in module name */
  if (dot == NULL) dot = modname; else dot++;
  /* set _PACKAGE as package name (full module name minus last part) */
  lua_pushlstring(L, modname, (size_t)(dot - modname));
  lua_setfield(L, -2, "_PACKAGE");
}

static int lj_cf_package_module(lua_State *L)
{
  const char *modname = luaL_checkstring(L, 1);
  int lastarg = (int)(L->top - L->base);
  luaL_pushmodule(L, modname, 1);
  lua_getfield(L, -1, "_NAME");
  if (!lua_isnil(L, -1)) {  /* Module already initialized? */
    lua_pop(L, 1);
  } else {
    lua_pop(L, 1);
    modinit(L, modname);
  }
  lua_pushvalue(L, -1);
  setfenv(L);
  dooptions(L, lastarg);
  return LJ_52;
}
#endif

static int lj_cf_package_seeall(lua_State *L)
{
  luaL_checktype(L, 1, LUA_TTABLE);
  if (!lua_getmetatable(L, 1)) {
    lua_createtable(L, 0, 1); /* create new metatable */
    lua_pushvalue(L, -1);
    lua_setmetatable(L, 1);
  }
  lua_pushvalue(L, LUA_GLOBALSINDEX);
  lua_setfield(L, -2, "__index");  /* mt.__index = _G */
  return 0;
}

/* ------------------------------------------------------------------------ */

#define AUXMARK		"\1"

#if LJ_ENABLE_DEBUG
static void setpath(lua_State *L, const char *fieldname, const char *envname,
		    const char *def, int noenv)
{
#if LJ_TARGET_CONSOLE
  const char *path = NULL;
  UNUSED(envname);
#else
  const char *path = getenv(envname);
#endif
  if (path == NULL || noenv) {
    lua_pushstring(L, def);
  } else {
    path = luaL_gsub(L, path, LUA_PATHSEP LUA_PATHSEP,
			      LUA_PATHSEP AUXMARK LUA_PATHSEP);
    luaL_gsub(L, path, AUXMARK, def);
    lua_remove(L, -2);
  }
  setprogdir(L);
  lua_setfield(L, -2, fieldname);
}
#endif

static const luaL_Reg package_lib[] = {
  { "seeall",	lj_cf_package_seeall },
  { NULL, NULL }
};

static const luaL_Reg package_global[] = {
#if LJ_ENABLE_DEBUG
  { "module",	lj_cf_package_module },
#endif
  { "require",	lj_cf_package_require },
  { NULL, NULL }
};

static const lua_CFunction package_loaders[] =
{
  lj_cf_package_loader_preload,
  lj_cf_package_loader_lua,
  NULL
};

LUALIB_API int luaopen_package(lua_State *L)
{
  int i;
#if LJ_ENABLE_DEBUG
  int noenv;
#endif
  luaL_register(L, LUA_LOADLIBNAME, package_lib);
  lua_copy(L, -1, LUA_ENVIRONINDEX);
  lua_createtable(L, sizeof(package_loaders)/sizeof(package_loaders[0])-1, 0);
  for (i = 0; package_loaders[i] != NULL; i++) {
    lj_lib_pushcf(L, package_loaders[i], 1);
    lua_rawseti(L, -2, i+1);
  }
#if LJ_52
  lua_pushvalue(L, -1);
  lua_setfield(L, -3, "searchers");
#endif
  lua_setfield(L, -2, "loaders");
#if LJ_ENABLE_DEBUG
  lua_getfield(L, LUA_REGISTRYINDEX, "LUA_NOENV");
  noenv = lua_toboolean(L, -1);
  lua_pop(L, 1);
  setpath(L, "path", LUA_PATH, LUA_PATH_DEFAULT, noenv);
  setpath(L, "cpath", LUA_CPATH, LUA_CPATH_DEFAULT, noenv);
  lua_pushliteral(L, LUA_PATH_CONFIG);
  lua_setfield(L, -2, "config");
#endif
  luaL_findtable(L, LUA_REGISTRYINDEX, "_LOADED", 16);
  lua_setfield(L, -2, "loaded");
  luaL_findtable(L, LUA_REGISTRYINDEX, "_PRELOAD", 4);
  lua_setfield(L, -2, "preload");
  lua_pushvalue(L, LUA_GLOBALSINDEX);
  luaL_register(L, NULL, package_global);
  lua_pop(L, 1);
  return 1;
}

