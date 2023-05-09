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
	/*printf("aio callback received!\n");*/
	struct callback_info *ci = (struct callback_info*)v;
	lua_State *L = ci->L;
	/*printf("About to lock lua state in callback\n");*/
	nng_mtx_lock(ci->lmutex);//lock the lua state
	/*printf("Done locking lua state in callback\n");*/
	int err = nng_aio_result(ci->aio);
	if(err != 0){
		/*printf("This callback was canceled or timed out: %d: %s\n", err, nng_strerror(err));*/
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
	/*printf("About to unlock lua state in callback\n");*/
	nng_mtx_unlock(ci->lmutex);
	/*printf("About to lock condition mutex in callback\n");*/
	nng_mtx_lock(ci->cmutex);
	/*printf("About wake condition\n");*/
	nng_cv_wake(ci->cv);
	/*printf("Done wake condition\n");*/
	nng_mtx_unlock(ci->cmutex);
	/*printf("Done with callback\n");*/
}

//recv_any(socket1, socket2, ...) :: {socket = message}
int lnng_aio_recv(lua_State *L){
	nng_mtx *luamtx, *callbackmtx;
	int err = nng_mtx_alloc(&luamtx);
	err |= nng_mtx_alloc(&callbackmtx);
	/*err |= nng_mtx_alloc(&setupmtx);*/
	if(err != 0){
		/*printf("Something when wrong when allocating mutexes\n");*/
		lua_pushboolean(L,0);
		lua_pushstring(L,nng_strerror(err));
		return 2;
	}
	int argv = lua_gettop(L);
	/*printf("Receiving any on %d sockets\n",argv);*/
	struct callback_info **cis = (struct callback_info**)malloc(sizeof(struct callback_info*) * argv);
	nng_mtx_lock(luamtx);
	/*printf("Locked lua state\n");*/
	nng_cv *cv;
	nng_cv_alloc(&cv, callbackmtx);
	/*nng_mtx_lock(callbackmtx);//wait for one of the callbacks to happen*/
	/*printf("Callback 1 happened\n");*/
	for(int i = 0; i < argv; i++){
		/*printf("\tSetting up async %d\n", i);*/
		nng_socket *sock = tosocket(L,-1);
		int sref = luaL_ref(L,LUA_REGISTRYINDEX);
		/*printf("\tGot socket ref %d\n", sref);*/
		cis[i] = (struct callback_info*)malloc(sizeof(struct callback_info));
		struct callback_info *ci = cis[i];
		/*printf("\tLooking at ci %p\n",ci);*/
		ci->L = L;
		ci->lmutex = luamtx;
		ci->cmutex = callbackmtx;
		ci->socketref = sref;
		ci->completed = 0;
		ci->cv = cv;
		/*printf("\tAbout to alloc aio\n");*/
		nng_aio_alloc(&(ci->aio), push_callback, ci);
		/*printf("\tAllocated aio\n");*/
		/*printf("\tEverything else set on callback info\n");*/
		nng_recv_aio(*sock, ci->aio);
		/*printf("\tSet up async receive %d\n",i);*/
	}
	lua_newtable(L);//table that will hold [socket] = message
	/*printf("About to unlock lua state\n");*/
	nng_mtx_unlock(luamtx);
	/*printf("Unlocked lua state\n");*/
	/*nng_mtx_lock(callbackmtx);//was unlocked by the callback, luamtx is locked at this point*/
	int complete = 0;
	nng_mtx_lock(callbackmtx);
	while(complete == 0){
		for(int i = 0; i < argv; i++){
			struct callback_info *ci = cis[i];
			if(ci->completed > 0){
				/*printf("At least 1 completed! breaking!\n");*/
				complete = 1;
				goto found;
			}
		}
		/*printf("About to wait\n");*/
		nng_cv_wait(cv);
		/*printf("Done waiting, complete is: %d\n", complete);*/
	}
	found:
	/*printf("Callback 2 happened\n");*/
	nng_mtx_unlock(callbackmtx);
	/*printf("Callback done\n");*/
	for(int i = 0; i < argv; i++){
		struct callback_info *ci = cis[i];
		/*printf("About to stop aio %d\n",ci->aio);*/
		/*nng_aio_cancel(ci->aio);*/
		luaL_unref(L,LUA_REGISTRYINDEX,ci->socketref);
		nng_aio_stop(ci->aio);
		nng_aio_free(ci->aio);
		free(ci);
		/*printf("Stopped aio %d\n",ci->aio);*/
	}
	free(cis);
	/*printf("Freeing things\n");*/
	nng_cv_free(cv);
	nng_mtx_free(callbackmtx);
	//nng_mtx_unlock(luamtx);//mutexes must not be locked when they are freed
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
