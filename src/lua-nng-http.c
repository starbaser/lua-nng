#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define NNG_STATIC_LIB

#include <nng/nng.h>

#include <nng/transport/inproc/inproc.h>
#include <nng/transport/ipc/ipc.h>
#include <nng/transport/tcp/tcp.h>
#include <nng/transport/tls/tls.h>
#include <nng/transport/zerotier/zerotier.h>

#include <nng/protocol/pair1/pair.h>
#include <nng/protocol/bus0/bus.h>
#include <nng/protocol/pubsub0/pub.h>
#include <nng/protocol/pubsub0/sub.h>
#include <nng/protocol/pipeline0/pull.h>
#include <nng/protocol/pipeline0/push.h>
#include <nng/protocol/reqrep0/req.h>
#include <nng/protocol/reqrep0/rep.h>
#include <nng/protocol/survey0/respond.h>
#include <nng/protocol/survey0/survey.h>

#include "lua-nng-common.h"

void handle_callback(nng_aio *aio){
	
}

//handler_alloc(string path,function callback) :: http_handler 
int lnng_http_handler_alloc(lua_State *L){

}

static const struct luaL_Reg nng_http_handler_m[] = {
	{NULL, NULL}
};

static const struct luaL_Reg nng_http_f[] = {
	{"handler_alloc",lnng_http_handler_alloc},
	{NULL, NULL}
};

int luaopen_nng_http(lua_State *L){
	luaL_newmetatable(L,"nng.http.handler");
	luaL_newlib(L,nng_http_handler_m);
	lua_setfield(L,-2,"__index");
	lua_pop(L,1);
	
	luaL_newlib(L,nng_http_f);
	return 1;
}
