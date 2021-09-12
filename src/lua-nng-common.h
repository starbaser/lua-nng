#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

/*
Lua version independence 5.1+
*/
#ifndef luaL_newlib
	#define luaL_newlib(l,r) luaL_register(l,NULL,r)
#endif
#ifndef luaL_setmetatable
	#define luaL_setmetatable(l,s) luaL_getmetatable(l,s); lua_setmetatable(l,-2);
#endif

// lua runtime's traceback function
int traceback (lua_State *L);
