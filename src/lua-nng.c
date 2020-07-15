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

#define OPEN(name)\
	int lnng_ ## name ## _open(lua_State *L){\
		nng_socket *s = (nng_socket*)lua_newuserdata(L,sizeof(nng_socket));\
		int err = nng_ ## name ## _open(s);\
		if(err == 0){\
			luaL_setmetatable(L,"nng.socket");\
			return 1;\
		}else{\
			lua_pushboolean(L,0);\
			lua_pushstring(L,nng_strerror(err));\
			return 2;\
		}\
	}

OPEN(bus0);
OPEN(pair1);
OPEN(pub0);
OPEN(sub0);
OPEN(pull0);
OPEN(push0);
OPEN(req0);
OPEN(rep0);
OPEN(surveyor0);
OPEN(respondent0);

nng_socket* tosocket(lua_State *L, int offset){
	luaL_checkudata(L,offset,"nng.socket");
	return (nng_socket*)lua_touserdata(L,offset);
}

//socket:listen(url[, flags]) :: listener
int lnng_listen(lua_State *L){
	int argc = lua_gettop(L);
	int flags = 0;
	nng_socket *sock = tosocket(L,1);
	const char *url = luaL_checkstring(L,2);
	if(argc >= 3){
		flags = luaL_checkinteger(L,3);
	}
	lua_pop(L,argc);
	nng_listener *lp = (nng_listener*)lua_newuserdata(L,sizeof(nng_listener));
	int err = nng_listen(*sock, url, lp, flags);
	if(err == 0){
		luaL_setmetatable(L,"nng.listener");
		return 1;
	}else{
		lua_pushboolean(L,0);
		lua_pushstring(L,nng_strerror(err));
		return 2;
	}
}

//socket:dial(url[, flags]) :: dialer
int lnng_dial(lua_State *L){
	int argc = lua_gettop(L);
	int flags = 0;
	nng_socket *sock = tosocket(L,1);
	const char *url = luaL_checkstring(L,2);
	if(argc >= 3){
		flags = luaL_checkinteger(L,3);
	}
	lua_pop(L,argc);
	nng_dialer *dp = (nng_dialer*)lua_newuserdata(L,sizeof(nng_dialer));
	int err = nng_dial(*sock, url, dp, flags);
	if(err == 0){
		luaL_setmetatable(L,"nng.dialer");
		return 1;
	}else{
		lua_pushboolean(L,0);
		lua_pushstring(L,nng_strerror(err));
		return 2;
	}
}

//socket:send("data"[, flags])
int lnng_send(lua_State *L){
	int argc = lua_gettop(L);
	int flags = 0;
	nng_socket *sock = tosocket(L,1);
	size_t datasize;
	const char *data = luaL_checklstring(L,2,&datasize);
	if(argc >= 3){
		flags = luaL_checkinteger(L,3);
	}
	lua_pop(L,argc);
	int err = nng_send(*sock, (void*)data, datasize, flags);
	if(err == 0){
		lua_pushboolean(L,1);
		return 1;
	}else{
		lua_pushboolean(L,0);
		lua_pushstring(L,nng_strerror(err));
		return 2;
	}
}

//socket:recv([flags])
int lnng_recv(lua_State *L){
	int argc = lua_gettop(L);
	int flags = NNG_FLAG_ALLOC; //don't support zero copy
	nng_socket *sock = tosocket(L,1);
	if(argc >= 2){
		flags += luaL_checkinteger(L,2);
	}
	char *data = NULL;
	size_t datasize;
	int err = nng_recv(*sock, &data, &datasize, flags);
	if(err == 0){
		lua_pushlstring(L,data,datasize);
		nng_free(data,datasize);
		return 1;
	}else{
		lua_pushboolean(L,0);
		lua_pushstring(L,nng_strerror(err));
		return 2;
	}
}

//socket:close()
int lnng_socket_close(lua_State *L){
	nng_socket *sock = tosocket(L,1);
	int err = nng_close(*sock);
	lua_pop(L,1);
	return 0;
}

//close(ud_dialer)
int lnng_dialer_close(lua_State *L){
	nng_dialer *dp = (nng_dialer*)lua_touserdata(L,1);
	nng_dialer_close(*dp);
	lua_pop(L,1);
	return 0;
}

//close(ud_listener)
int lnng_listener_close(lua_State *L){
	nng_listener *lp = (nng_listener*)lua_touserdata(L,1);
	int err = nng_listener_close(*lp);
	lua_pop(L,1);
	return 0;
}

//subscribe(socket,"topic")
int lnng_subscribe(lua_State *L){
	nng_socket *sock = tosocket(L,1);
	size_t size;
	const char *topic = luaL_checklstring(L,2,&size);
	lua_pop(L,2);
	int err = nng_socket_set(*sock,NNG_OPT_SUB_SUBSCRIBE,topic,size);
	if(err == 0){
		lua_pushboolean(L,1);
		return 1;
	}else{
		lua_pushboolean(L,0);
		lua_pushstring(L,nng_strerror(err));
		return 2;
	}
}

//unsubscribe(socket,"topic")
int lnng_unsubscribe(lua_State *L){
	nng_socket *sock = tosocket(L,1);
	const char *topic = lua_tostring(L,2);
	lua_pop(L,2);
	int err = nng_socket_set_string(*sock,NNG_OPT_SUB_UNSUBSCRIBE,topic);
	if(err == 0){
		lua_pushboolean(L,1);
		return 1;
	}else{
		lua_pushboolean(L,0);
		lua_pushstring(L,nng_strerror(err));
		return 2;
	}
}

static const struct luaL_Reg nng_dialer_m[] = {
	{NULL, NULL}
};

static const struct luaL_Reg nng_listener_m[] = {

	{NULL, NULL}
};

static const struct luaL_Reg nng_socket_sub_m[] = {
	{"subscribe", lnng_subscribe},
	{"unsubscribe", lnng_unsubscribe},
	{NULL, NULL}
};

static const struct luaL_Reg nng_socket_m[] = {
	{"dial", lnng_dial},
	{"listen", lnng_listen},
	{"send", lnng_send},
	{"recv", lnng_recv},

	//pub/sub only
	{"subscribe",lnng_subscribe},
	{"unsubscribe",lnng_unsubscribe},
	{NULL, NULL}
};

static const struct luaL_Reg nng_f[] = {
	{"bus0_open", lnng_bus0_open},
	{"pair1_open", lnng_pair1_open},
	{"pub0_open", lnng_pub0_open},
	{"sub0_open", lnng_sub0_open},
	{"pull0_open", lnng_pull0_open},
	{"push0_open", lnng_push0_open},
	{"req0_open", lnng_req0_open},
	{"rep0_open", lnng_rep0_open},
	{"surveyor0_open",lnng_surveyor0_open},
	{"respondent0_open",lnng_respondent0_open},
	{NULL, NULL}
};


#define flag(name) lua_pushnumber(L,name); lua_setfield(L,-2,#name);
int luaopen_nng(lua_State *L){
	luaL_newmetatable(L,"nng.socket");
	luaL_newlib(L,nng_socket_m);
	lua_setfield(L,-2,"__index");
	lua_pushcfunction(L,lnng_socket_close);
	lua_setfield(L,-2,"__gc");
	lua_pop(L,1);

	luaL_newmetatable(L,"nng.dialer");
	luaL_newlib(L,nng_dialer_m);
	lua_setfield(L,-2,"__index");
	lua_pushcfunction(L,lnng_dialer_close);
	lua_setfield(L,-2,"__gc");
	lua_pop(L,1);

	luaL_newmetatable(L,"nng.listener");
	luaL_newlib(L,nng_listener_m);
	lua_setfield(L,-2,"__index");
	lua_pushcfunction(L,lnng_listener_close);
	lua_setfield(L,-2,"__gc");
	lua_pop(L,1);

	luaL_newlib(L,nng_f);
	flag(NNG_FLAG_NONBLOCK);
	flag(NNG_FLAG_ALLOC);
	return 1;
}
