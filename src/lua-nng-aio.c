#include <stdlib.h>
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

#include <nng/supplemental/util/platform.h>

#include "lua-nng-common.h"
#include "lua-nng.h"

struct lnng_aio{
	lua_State *L;
	int func;
	int args;
	nng_aio *aio;
};

void lcallback(void *lfaa){
	printf("aio callback running\n");
	struct lnng_aio *l = (struct lnng_aio*)lfaa;
	lua_State *L = l->L;
	printf("At the beginning we have: %d\n",lua_gettop(L));
	for(int i = 1; i <= lua_gettop(L); i++){
		printf("-%d - %s\n",i,lua_typename(L,lua_type(L,-i)));
		lua_getglobal(L,"print");
		lua_pushvalue(L,i);
		lua_call(L,1,0);
	}
	lua_getglobal(L,"table");//{table}
	lua_pushcfunction(L,traceback);//{table},traceback()
	lua_rawgeti(L,LUA_REGISTRYINDEX, l->func);//{table},traceback(),callb()
	printf("foo\n");
	lua_getfield(L,-3,"unpack");//{table},traceback(),callb(),table.unpack()
	printf("unpacked\n");
	printf("unpack, -1 - %s\n",lua_typename(L,lua_type(L,-1)));
	printf("Args was: %d\n",l->args);
	lua_rawgeti(L,LUA_REGISTRYINDEX, l->args);//{table},traceback(),callb(),table.unpack(),{args}
	printf("got args\n");
	printf("-2 - %s\n",lua_typename(L,lua_type(L,-2)));
	printf("-1 - %s\n",lua_typename(L,lua_type(L,-1)));
	lua_call(L, 1, LUA_MULTRET);//{table},traceback(), callb(), args...
	printf("bar\n");
	printf("Top: %d\n",lua_gettop(L));
	for(int i = 1; i <= lua_gettop(L); i++){
		printf("-%d - %s\n",i,lua_typename(L,lua_type(L,-i)));
	}
	int numargs = lua_gettop(L) - 3;
	printf("Nargs:%d\n",numargs);
	lua_pcall(L, numargs, 0, 2);//{table},traceback()
	printf("Finished with everything, poping last 2 and returning\n");
	lua_pop(L,2);//
}

//nng.aio_alloc(callback(), args...) :: nng.aio
int lnng_aio_alloc(lua_State *L){
	int argc = lua_gettop(L);//callback(), args...
	lua_createtable(L,argc - 1, 0);//callback(), args..., {}
	for(int i = 1; i < argc; i++){
		printf("Adding element %d to arg table (%s)\n",i,lua_typename(L,lua_type(L,i + 1)));
		lua_pushnumber(L, i);
		lua_getglobal(L,"print");
		lua_pushvalue(L, i + 1);
		lua_call(L,1,0);
		lua_pushvalue(L, i + 1);
		lua_settable(L, -3);
	}
	int argtbl = luaL_ref(L, LUA_REGISTRYINDEX);//callback(), args...
	lua_pop(L, argc - 1);//callback()
	int func = luaL_ref(L, LUA_REGISTRYINDEX);//
	
	struct lnng_aio *lfaa = (struct lnng_aio*)lua_newuserdata(L,sizeof(struct lnng_aio));//userdata
	lfaa->L = L;
	lfaa->args = argtbl;
	lfaa->func = func;
	nng_aio *aio;
	int err = nng_aio_alloc(&aio, lcallback, (void*)lfaa);
	lfaa->aio = aio;
	if(err == 0){
		printf("After aio_alloc we have:%d",lua_gettop(L));
		luaL_setmetatable(L,"nng.aio.struct");
		return 1;
	}else{
		lua_pop(L,1);
		lua_pushboolean(L,0);
		lua_pushstring(L,nng_strerror(err));
		return 2;
	}
}
struct nng_aio* toaio(lua_State *L, int index){
	struct lnng_aio* l = (struct lnng_aio*)luaL_checkudata(L,index,"nng.aio.struct");
	return l->aio;
}

//sleep(duration, aio)
int lnng_aio_sleep(lua_State *L){
	printf("sleep called\n");
	int duration = luaL_checkinteger(L,1);
	printf("Duration was %d\n");
	nng_aio *aio = toaio(L,2);
	printf("Got aio: %p\n",aio);
	nng_sleep_aio(duration, aio);
	printf("did sleep\n");
	return 0;
}

struct callback_info {
	lua_State *L;
	nng_mtx *lmutex; //mutex for the lua state
	nng_mtx *cmutex;
	nng_cv *cv;
	int socketref; //lua refrence to this socket
	nng_aio *aio;
	int completed;
};

void push_callback(void *v){
	printf("aio callback received!\n");
	struct callback_info *ci = (struct callback_info*)v;
	lua_State *L = ci->L;
	printf("About to lock lua state in callback\n");
	nng_mtx_lock(ci->lmutex);//lock the lua state
	printf("Done locking lua state in callback\n");
	int err = nng_aio_result(ci->aio);
	if(err != 0){
		printf("This callback was canceled or timed out: %d: %s\n", err, nng_strerror(err));
		nng_mtx_unlock(ci->lmutex);
		return;
	}
	lua_rawgeti(L,LUA_REGISTRYINDEX,ci->socketref);//push socket
	luaL_unref(L,LUA_REGISTRYINDEX,ci->socketref);//free the reference
	nng_msg *msg = nng_aio_get_msg(ci->aio);
	size_t len = nng_msg_len(msg);
	void *body = nng_msg_body(msg);
	lua_pushlstring(L,(const char*)body, len);//push the message
	nng_msg_free(msg);
	ci->completed = 1;
	lua_settable(L,1);
	printf("About to unlock lua state in callback\n");
	nng_mtx_unlock(ci->lmutex);
	printf("About to lock condition mutex in callback\n");
	nng_mtx_lock(ci->cmutex);
	printf("About wake condition\n");
	nng_cv_wake(ci->cv);
	printf("Done wake condition\n");
	nng_mtx_unlock(ci->cmutex);
	printf("Done with callback\n");
}

//TODO: there is a wierd bug here:
//If multiple sockets receive a message at the same time, one or more of those
//messages can be thrown out becuase of the canceling of the async recieves.
//recv_any(socket1, socket2, ...) :: socket | false, message | errmsg,
int lnng_aio_recv(lua_State *L){
	nng_mtx *luamtx, *callbackmtx, *setupmtx;
	int err = nng_mtx_alloc(&luamtx);
	err |= nng_mtx_alloc(&callbackmtx);
	err |= nng_mtx_alloc(&setupmtx);
	if(err != 0){
		printf("Something when wrong when allocating mutexes\n");
		lua_pushboolean(L,0);
		lua_pushstring(L,nng_strerror(err));
		return 2;
	}
	int argv = lua_gettop(L);
	printf("Receiving any on %d sockets\n",argv);
	struct callback_info **cis = (struct callback_info**)malloc(sizeof(struct callback_info*) * argv);
	nng_mtx_lock(luamtx);
	printf("Locked lua state\n");
	nng_cv *cv;
	nng_cv_alloc(&cv, callbackmtx);
	/*nng_mtx_lock(callbackmtx);//wait for one of the callbacks to happen*/
	printf("Callback 1 happened\n");
	for(int i = 0; i < argv; i++){
		printf("\tSetting up async %d\n", i);
		nng_socket *sock = tosocket(L,-1);
		int sref = luaL_ref(L,LUA_REGISTRYINDEX);
		printf("\tGot socket ref %d\n", sref);
		cis[i] = (struct callback_info*)malloc(sizeof(struct callback_info));
		struct callback_info *ci = cis[i];
		printf("\tLooking at ci %p\n",ci);
		ci->L = L;
		ci->lmutex = luamtx;
		ci->cmutex = callbackmtx;
		ci->socketref = sref;
		ci->completed = 0;
		ci->cv = cv;
		printf("\tAbout to alloc aio\n");
		nng_aio_alloc(&(ci->aio), push_callback, ci);
		printf("\tAllocated aio\n");
		printf("\tEverything else set on callback info\n");
		nng_recv_aio(*sock, ci->aio);
		printf("\tSet up async receive %d\n",i);
	}
	lua_newtable(L);//table that will hold [socket] = message
	printf("About to unlock lua state\n");
	nng_mtx_unlock(luamtx);
	printf("Unlocked lua state\n");
	/*nng_mtx_lock(callbackmtx);//was unlocked by the callback, luamtx is locked at this point*/
	int complete = 0;
	nng_mtx_lock(callbackmtx);
	while(complete == 0){
		for(int i = 0; i < argv; i++){
			struct callback_info *ci = cis[i];
			if(ci->completed > 0){
				printf("At least 1 completed! breaking!\n");
				complete = 1;
				goto found;
			}
		}
		printf("About to wait\n");
		nng_cv_wait(cv);
		printf("Done waiting, complete is: %d\n", complete);
	}
	found:
	printf("Callback 2 happened\n");
	nng_mtx_unlock(callbackmtx);
	printf("Callback done\n");
	for(int i = 0; i < argv; i++){
		struct callback_info *ci = cis[i];
		printf("About to stop aio %d\n",ci->aio);
		/*nng_aio_cancel(ci->aio);*/
		nng_aio_stop(ci->aio);
		nng_aio_free(ci->aio);
		free(ci);
		printf("Stopped aio %d\n",ci->aio);
	}
	free(cis);
	printf("Freeing things\n");
	nng_cv_free(cv);
	nng_mtx_free(callbackmtx);
	nng_mtx_unlock(luamtx);//mutexes must not be locked when they are freed
	nng_mtx_free(luamtx);
	/*printf("Done freeing everything, returning...\n");*/
	return 1;
}

static const struct luaL_Reg nng_aio_handler_m[] = {
	{NULL, NULL}
};

static const struct luaL_Reg nng_aio_mutex_m[] = {
	{NULL, NULL}
};


static const struct luaL_Reg nng_http_f[] = {
	{"alloc",lnng_aio_alloc},
	{"recv_any",lnng_aio_recv},
	{NULL, NULL}
};

int luaopen_nng_aio(lua_State *L){
	luaL_newmetatable(L,"nng.aio.struct");
	luaL_newlib(L,nng_aio_handler_m);
	lua_setfield(L,-2,"__index");
	lua_pop(L,1);

	luaL_newmetatable(L,"nng.aio.mutex");
	luaL_newlib(L,nng_aio_mutex_m);
	lua_setfield(L,-2,"__index");
	lua_pop(L,1);
	
	luaL_newlib(L,nng_http_f);
	return 1;
}
