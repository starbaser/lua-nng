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

#include <string.h>
#include "lua-nng-aio.h"

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

//sleep(ms)
int lnng_msleep(lua_State *L){
	int ms = luaL_checkinteger(L,1);
	nng_msleep(ms);
	lua_pop(L,1);
	return 0;
}

nng_socket* tosocket(lua_State *L, int offset){
	luaL_checkudata(L,offset,"nng.socket");
	return (nng_socket*)lua_touserdata(L,offset);
}

nng_listener* tolistener(lua_State *L, int offset){
	luaL_checkudata(L,offset,"nng.listener");
	return (nng_listener*)lua_touserdata(L,offset);
}

nng_dialer* todialer(lua_State *L, int offset){
	luaL_checkudata(L,offset,"nng.dialer");
	return (nng_dialer*)lua_touserdata(L,offset);
}

nng_sockaddr* tosockaddr(lua_State *L, int offset){
	luaL_checkudata(L,offset,"nng.sockaddr");
	return (nng_sockaddr*)lua_touserdata(L,offset);
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
	/*printf("Garbage collecting socket...");*/
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
	size_t size;
	const char *topic = luaL_checklstring(L,2,&size);
	lua_pop(L,2);
	int err = nng_socket_set(*sock,NNG_OPT_SUB_UNSUBSCRIBE,topic,size);
	if(err == 0){
		lua_pushboolean(L,1);
		return 1;
	}else{
		lua_pushboolean(L,0);
		lua_pushstring(L,nng_strerror(err));
		return 2;
	}
}

//Option types
#define SOCKET_OPTION_SET(L, socket, flag, matches, ntype, gets, sets) \
	if(strcmp(flag, matches) == 0){\
		ntype value = (ntype)gets(L,3);\
		int err = sets(*socket, flag, value);\
		lua_pop(L,lua_gettop(L));\
		if(err != 0){\
			lua_pushboolean(L,0);\
			lua_pushfstring(L,nng_strerror(err));\
			return 2;\
		}else{\
			return 0;\
		}\
	}

#define SOCKET_OPTION_GET(L, socket, flag, matches, ntype, gets, pushes) \
	if(strcmp(flag, matches) == 0){\
		ntype value;\
		int err = gets(*socket, flag, &value);\
		lua_pop(L,lua_gettop(L));\
		if(err != 0){\
			lua_pushboolean(L,0);\
			lua_pushfstring(L,nng_strerror(err));\
			return 2;\
		}else{\
			pushes(L,value);\
			return 1;\
		}\
	}

//TODO
//set(listener,"flag",value)
int lnng_listener_set(lua_State *L){
	return 0;
}

int lnng_sockaddr_get(lua_State *L){
	nng_sockaddr *sa = tosockaddr(L,1);
	unsigned int f = sa->s_family;
	const char *field = luaL_checkstring(L,2);
	lua_pop(L,2);
	printf("Getting %s from sockaddr\n",field);
	if(strcmp(field,"type") == 0){
		lua_pushnumber(L,f);
		return 1;
	}
	if(f == NNG_AF_UNSPEC){
		printf("Unspec\n");
		lua_pushnil(L);
		return 1;
	}else if(f == NNG_AF_INPROC){
		printf("Inproc\n");
		nng_sockaddr_inproc sai = sa->s_inproc;
		if(strcmp(field,"name") == 0){
			lua_pushstring(L, sai.sa_name);
			return 1;
		}
	}else if(f == NNG_AF_IPC){
		printf("IPC\n");
		nng_sockaddr_ipc sai = sa->s_ipc;
		if(strcmp(field,"path") == 0){
			lua_pushstring(L, sai.sa_path);
			return 1;
		}
	}else if(f == NNG_AF_INET){
		printf("Inet\n");
		nng_sockaddr_in sai = sa->s_in;
		if(strcmp(field,"addr") == 0){
			lua_pushnumber(L, sai.sa_addr);
			return 1;
		}else if(strcmp(field,"port") == 0){
			lua_pushnumber(L, sai.sa_port);
			return 1;
		}
	}else if(f == NNG_AF_INET6){
		nng_sockaddr_in6 sai = sa->s_in6;
		if(strcmp(field,"addr") == 0){
			lua_pushlstring(L,sai.sa_addr,16);
			return 1;
		}else if(strcmp(field,"port") == 0){
			lua_pushnumber(L,sai.sa_port);
			return 1;
		}
	}else if(f == NNG_AF_ZT){
		nng_sockaddr_zt sai = sa->s_zt;
		if(strcmp(field,"nwid") == 0){
			lua_pushnumber(L,sai.sa_nwid);
			return 1;
		}else if(strcmp(field,"nodeid") == 0){
			lua_pushnumber(L,sai.sa_nodeid);
			return 1;
		}else if(strcmp(field,"port") == 0){
			lua_pushnumber(L,sai.sa_port);
			return 1;
		}
	}
}

//set(socket, "flag", value)
int lnng_socket_set(lua_State *L){
	nng_socket *sock = tosocket(L,1);
	const char *flag = luaL_checkstring(L,2);
	//NNG_OPT_LOCADDR - read-only
	SOCKET_OPTION_SET(L, sock, flag, NNG_OPT_RECONNMINT, nng_duration, luaL_checkinteger, nng_socket_set_ms);
	SOCKET_OPTION_SET(L, sock, flag, NNG_OPT_RECONNMAXT, nng_duration, luaL_checkinteger, nng_socket_set_ms);
	SOCKET_OPTION_SET(L, sock, flag, NNG_OPT_RECVBUF, int, luaL_checkinteger, nng_socket_set_int);
	//NNG_OPT_RECVFD - read-only
	SOCKET_OPTION_SET(L, sock, flag, NNG_OPT_RECVMAXSZ, size_t, luaL_checkinteger, nng_socket_set_uint64);
	SOCKET_OPTION_SET(L, sock, flag, NNG_OPT_RECVTIMEO, nng_duration, luaL_checkinteger, nng_socket_set_ms);
	//NNG_OPT_REMADDR - read-only
	SOCKET_OPTION_SET(L, sock, flag, NNG_OPT_SENDBUF, int, luaL_checkinteger, nng_socket_set_int);
	//NNG_OPT_SENDFD - read-only
	SOCKET_OPTION_SET(L, sock, flag, NNG_OPT_SENDTIMEO, nng_duration, luaL_checkinteger, nng_socket_set_ms);
	SOCKET_OPTION_SET(L, sock, flag, NNG_OPT_SOCKNAME, const char*, luaL_checkstring, nng_socket_set_string);
	SOCKET_OPTION_SET(L, sock, flag, NNG_OPT_MAXTTL, int, luaL_checkinteger, nng_socket_set_int);
	//NNG_OPT_UR - read-only
	//NNG_OPT_PROTO - read-only
	//NNG_OPT_PEER - read-only
	//NNG_OPT_PROTONAME - read-only
	//NNG_OPT_PEERNAME - read-only
	
	//TCP options
	SOCKET_OPTION_SET(L, sock, flag, NNG_OPT_TCP_NODELAY, bool, lua_toboolean, nng_socket_set_bool);
	SOCKET_OPTION_SET(L, sock, flag, NNG_OPT_TCP_KEEPALIVE, bool, lua_toboolean, nng_socket_set_bool);
	//NNG_OPT_TCP_BOUND_PORT - read-only? documentation doesn't say it, but it would be wierd if we could write to it.
	
	//TLS options
	SOCKET_OPTION_SET(L, sock, flag, NNG_OPT_TLS_AUTH_MODE, int, luaL_checkinteger, nng_socket_set_int); //write-only
	SOCKET_OPTION_SET(L, sock, flag, NNG_OPT_TLS_CA_FILE, const char*, luaL_checkstring, nng_socket_set_string); //write-only
	SOCKET_OPTION_SET(L, sock, flag, NNG_OPT_TLS_CERT_KEY_FILE, const char*, luaL_checkstring, nng_socket_set_string); //write-only
	//SOCKET_OPTION_SET(L, sock, flag, NNG_OPT_TLS  TODO: NNG_OPT_TLS_CONFIG
	SOCKET_OPTION_SET(L, sock, flag, NNG_OPT_TLS_SERVER_NAME, const char*, luaL_checkstring, nng_socket_set_string);
	//NNG_OPT_TLS_VERIFIED - read-only
	
	//IPC options
	//NNG_OPT_IPC_PEER_GID - read-only
	//NNG_OPT_IPC_PEER_PID - read-only
	//NNG_OPT_IPC_PEER_UID - read-only
	//NNG_OPT_IPC_PEER_ZONEID - read-only
	SOCKET_OPTION_SET(L, sock, flag, NNG_OPT_IPC_PERMISSIONS, int, luaL_checkinteger, nng_socket_set_int);
	//TODO: NNG_OPT_IPC_SECURITY_DESCRIPTOR - windows-only, sets a pointer to a PSECURITY_DESCRIPTOR
	
	//PUB/SUB options
	SOCKET_OPTION_SET(L, sock, flag, NNG_OPT_SUB_PREFNEW, bool, lua_toboolean, nng_socket_set_bool);

	//REQ/REP options
	SOCKET_OPTION_SET(L, sock, flag, NNG_OPT_REQ_RESENDTIME, nng_duration, luaL_checkinteger, nng_socket_set_ms);

	//Survayor/respondent options
	SOCKET_OPTION_SET(L, sock, flag, NNG_OPT_SURVEYOR_SURVEYTIME, nng_duration, luaL_checkinteger, nng_socket_set_ms);
}

//get(socket,"flag",value)
int lnng_socket_get(lua_State *L){
	/*printf("Lua stack is %d\n",lua_gettop(L));*/
	nng_socket *sock = tosocket(L,1);
	const char *flag = luaL_checkstring(L,2);
	//NNG_OPT_LOCADDR - listeners, dialers, and connected pipes
	SOCKET_OPTION_GET(L, sock, flag, NNG_OPT_RAW, bool, nng_socket_get_bool, lua_pushboolean); //read-only
	SOCKET_OPTION_GET(L, sock, flag, NNG_OPT_RECONNMINT, nng_duration, nng_socket_get_ms, lua_pushinteger);//sockets and dialers, if both dialer and socket are set, dialer overrides socket
	SOCKET_OPTION_GET(L, sock, flag, NNG_OPT_RECONNMAXT, nng_duration, nng_socket_get_ms, lua_pushinteger); //sockets and dialers, if both dialer and socket are set, dialer overrides socket
	SOCKET_OPTION_GET(L, sock, flag, NNG_OPT_RECVBUF, int, nng_socket_get_int, lua_pushinteger);//socket only
	SOCKET_OPTION_GET(L, sock, flag, NNG_OPT_RECVFD, int, nng_socket_get_int, lua_pushinteger); //socket only, read-only
	SOCKET_OPTION_GET(L, sock, flag, NNG_OPT_RECVMAXSZ, size_t, nng_socket_get_size, lua_pushinteger);//sockets, dialers and listeners
	SOCKET_OPTION_GET(L, sock, flag, NNG_OPT_RECVTIMEO, nng_duration, nng_socket_get_ms, lua_pushinteger);//socket only
	if(strcmp(flag, NNG_OPT_REMADDR) == 0){
		lua_pop(L,lua_gettop(L));
		nng_sockaddr *sa = (nng_sockaddr*)lua_newuserdata(L,sizeof(nng_sockaddr));//{udata}
		int err = nng_socket_get_addr(*sock, NNG_OPT_REMADDR, sa);//{udata}
		if(err == 0){
			luaL_setmetatable(L,"nng.sockaddr");
			return 1;
		}else{
			return luaL_error(L,"Failed to lookup local address - %s",nng_strerror(err));
		}
	}
	SOCKET_OPTION_GET(L, sock, flag, NNG_OPT_SENDBUF, int, nng_socket_get_int, lua_pushinteger);
	SOCKET_OPTION_GET(L, sock, flag, NNG_OPT_SENDFD, int, nng_socket_get_int, lua_pushinteger); //read-only
	SOCKET_OPTION_GET(L, sock, flag, NNG_OPT_SENDTIMEO, nng_duration, nng_socket_get_ms, lua_pushinteger);
	SOCKET_OPTION_GET(L, sock, flag, NNG_OPT_SOCKNAME, char*, nng_socket_get_string, lua_pushstring);
	SOCKET_OPTION_GET(L, sock, flag, NNG_OPT_MAXTTL, int, nng_socket_get_int, lua_pushinteger);
	//SOCKET_OPTION_GET(L, sock, flag, NNG_OPT_URL, char*, nng_socket_get_string, lua_pushstring); //read-only for dialers, listeners, and pipes
	SOCKET_OPTION_GET(L, sock, flag, NNG_OPT_PROTO, int, nng_socket_get_int, lua_pushinteger); //read-only
	SOCKET_OPTION_GET(L, sock, flag, NNG_OPT_PEER, int, nng_socket_get_int, lua_pushinteger); //read-only
	SOCKET_OPTION_GET(L, sock, flag, NNG_OPT_PROTONAME, char*, nng_socket_get_string, lua_pushstring); //read-only
	SOCKET_OPTION_GET(L, sock, flag, NNG_OPT_PEERNAME, char*, nng_socket_get_string, lua_pushstring); //read-only

	//TCP options
	SOCKET_OPTION_GET(L, sock, flag, NNG_OPT_TCP_NODELAY, bool, nng_socket_get_bool, lua_pushboolean);
	SOCKET_OPTION_GET(L, sock, flag, NNG_OPT_TCP_KEEPALIVE, bool, nng_socket_get_bool, lua_pushboolean);
	SOCKET_OPTION_GET(L, sock, flag, NNG_OPT_TCP_BOUND_PORT, int, nng_socket_get_int, lua_pushinteger);

	//TLS options
	//NNG_OPT_TLS_MODE - write-only option
	//NNG_OPT_TLS_CA_FILE - write-only option
	//NNG_OPT_TLS_CERT_KEY_FILE - write-only option
	//TODO: NNG_OPT_TLS_CONFIG
	//NNG_OPT_TLS_SERVER_NAME - write-only option
	SOCKET_OPTION_GET(L, sock, flag, NNG_OPT_TLS_VERIFIED, bool, nng_socket_get_bool, lua_pushboolean); //read-only option
	
	//IPC options
	SOCKET_OPTION_GET(L, sock, flag, NNG_OPT_IPC_PEER_GID, uint64_t, nng_socket_get_uint64, lua_pushinteger); //read-only option
	SOCKET_OPTION_GET(L, sock, flag, NNG_OPT_IPC_PEER_PID, uint64_t, nng_socket_get_uint64, lua_pushinteger); //read-only option
	SOCKET_OPTION_GET(L, sock, flag, NNG_OPT_IPC_PEER_UID, uint64_t, nng_socket_get_uint64, lua_pushinteger); //read-only option
	SOCKET_OPTION_GET(L, sock, flag, NNG_OPT_IPC_PEER_ZONEID, uint64_t, nng_socket_get_uint64, lua_pushinteger); //read-only option on Solaris and illumos systems only
	//NNG_OPT_IPC_PERMISSIONS - write-only option
	//NNG_OPT_IPC_SECURITY_DESCRIPTOR - write-only option
	
	//PUB/SUB options
	SOCKET_OPTION_GET(L, sock, flag, NNG_OPT_SUB_PREFNEW, bool, nng_socket_get_bool, lua_pushboolean);

	//REQ/REP options
	SOCKET_OPTION_GET(L, sock, flag, NNG_OPT_REQ_RESENDTIME, nng_duration, nng_socket_get_ms, lua_pushinteger);

	//Survayor/respondent options
	SOCKET_OPTION_GET(L, sock, flag, NNG_OPT_SURVEYOR_SURVEYTIME, nng_duration, nng_socket_get_ms, lua_pushinteger);
	lua_pop(L,2);

	//If none of the above options matched, get the value from the metatable
	int type = luaL_getmetatable(L,"nng.socket_m");////{socket_m}
	/*luaL_newlib(L,nng_socket_m);*/
	lua_getfield(L,-1,"__index");
	lua_getfield(L,-1,flag);//{socket_m},{socket},any
	int ref = luaL_ref(L,LUA_REGISTRYINDEX);//{socket_m},{socket}
	lua_pop(L,2);//
	lua_rawgeti(L,LUA_REGISTRYINDEX,ref);//any
	return 1;
}

/*#define DIALER_OPTION_SET(L, socket, flag, matches, ntype, gets, sets) \*/
	/*if(strcmp(flag, matches) == 0){\*/
		/*ntype value = (ntype)gets(L,3);\*/
		/*int err = sets(*socket, flag, value);\*/
		/*lua_pop(L,lua_gettop(L));\*/
		/*if(err != 0){\*/
			/*lua_pushboolean(L,0);\*/
			/*lua_pushfstring(L,nng_strerror(err));\*/
			/*return 2;\*/
		/*}else{\*/
			/*return 0;\*/
		/*}\*/
	/*}*/

/*#define DIALER_OPTION_GET(L, dialer, flag, matches, ntype, gets, pushes) \*/
	/*if(strcmp(flag, matches) == 0){\*/
		/*ntype value;\*/
		/*int err = gets(*socket, flag, &value);\*/
		/*lua_pop(L,lua_gettop(L));\*/
		/*if(err != 0){\*/
			/*lua_pushboolean(L,0);\*/
			/*lua_pushfstring(L,nng_strerror(err));\*/
			/*return 2;\*/
		/*}else{\*/
			/*pushes(L,value);\*/
			/*return 1;\*/
		/*}\*/
	/*}*/


int lnng_dialer_get(lua_State *L){
	nng_dialer *dialer = todialer(L,1);
	const char *flag = luaL_checkstring(L,2);
	if(strcmp(flag, NNG_OPT_LOCADDR) == 0){
		printf("Looking for dialer's locaddr\n");
		lua_pop(L,lua_gettop(L));
		nng_sockaddr *sa = (nng_sockaddr*)lua_newuserdata(L,sizeof(nng_sockaddr));//{udata}
		int err = nng_dialer_get_addr(*dialer , NNG_OPT_LOCADDR, sa);//{udata}
		if(err == 0){
			luaL_setmetatable(L,"nng.sockaddr");
			return 1;
		}else{
			return luaL_error(L,"Failed to lookup local address - %s",nng_strerror(err));
		}
	}
	SOCKET_OPTION_GET(L, dialer, flag, NNG_OPT_RECONNMINT, nng_duration, nng_dialer_get_ms, lua_pushinteger);//sockets and dialers, if both dialer and socket are set, dialer overrides socket
	SOCKET_OPTION_GET(L, dialer, flag, NNG_OPT_RECONNMAXT, nng_duration, nng_dialer_get_ms, lua_pushinteger); //sockets and dialers, if both dialer and socket are set, dialer overrides socket
	SOCKET_OPTION_GET(L, dialer, flag, NNG_OPT_RECVMAXSZ, size_t, nng_dialer_get_size, lua_pushinteger);//sockets, dialers and listeners
	if(strcmp(flag, NNG_OPT_REMADDR) == 0){
		lua_pop(L,lua_gettop(L));
		nng_sockaddr *sa = (nng_sockaddr*)lua_newuserdata(L,sizeof(nng_sockaddr));//{udata}
		int err = nng_dialer_get_addr(*dialer, NNG_OPT_REMADDR, sa);//{udata}
		if(err == 0){
			luaL_setmetatable(L,"nng.sockaddr");
			return 1;
		}else{
			return luaL_error(L,"Failed to lookup remote address - %s",nng_strerror(err));
		}
	}
	SOCKET_OPTION_GET(L, dialer, flag, NNG_OPT_URL, char*, nng_dialer_get_string, lua_pushstring); //read-only for dialers, listeners, and pipes
	
	//If none of the above options matched, get the value from the metatable
	int type = luaL_getmetatable(L,"nng.dialer_m");////{socket_m}
	/*luaL_newlib(L,nng_socket_m);*/
	lua_getfield(L,-1,"__index");
	lua_getfield(L,-1,flag);//{socket_m},{socket},any
	int ref = luaL_ref(L,LUA_REGISTRYINDEX);//{socket_m},{socket}
	lua_pop(L,2);//
	lua_rawgeti(L,LUA_REGISTRYINDEX,ref);//any
	return 1;
}

int lnng_dialer_set(lua_State *L){
	
}

static const struct luaL_Reg nng_dialer_m[] = {
	{"close",lnng_dialer_close},
	{NULL, NULL}
};

static const struct luaL_Reg nng_listener_m[] = {
	{"close",lnng_listener_close},
	{NULL, NULL}
};

static const struct luaL_Reg nng_socket_m[] = {
	{"dial", lnng_dial},
	{"listen", lnng_listen},
	{"send", lnng_send},
	{"recv", lnng_recv},
	{"close", lnng_socket_close},

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
	{"sleep",lnng_msleep},
	{NULL, NULL}
};


#define flag(name) lua_pushnumber(L,name); lua_setfield(L,-2,#name);
#define option(name) lua_pushstring(L,name); lua_setfield(L,-2,#name);
int luaopen_nng(lua_State *L){
	luaL_newmetatable(L,"nng.socket_m");//{}
	luaL_newlib(L,nng_socket_m);//{},{socket_m}
	lua_setfield(L,-2,"__index");//{__index={socket_m}}
	lua_pop(L,1);//
	
	luaL_newmetatable(L,"nng.socket");//{}
	lua_pushcfunction(L,lnng_socket_get);//{},socket_get()
	lua_setfield(L,-2,"__index");//{__index = socket_get()}
	lua_pushcfunction(L,lnng_socket_close);//{__index = {socket_m}},close()
	lua_setfield(L,-2,"__gc");//{__index = {socket_m}, __gc = close()}
	lua_pushcfunction(L,lnng_socket_set);//{__index = {socket_m}, __gc = close()}, set()
	lua_setfield(L,-2,"__newindex");//{__index = {socket_m}, __gc = close(), __newindex = set()}
	lua_pop(L,1);

	luaL_newmetatable(L,"nng.dialer");//{}
	lua_pushcfunction(L,lnng_dialer_get);//{},dialer_get()
	lua_setfield(L,-2,"__index");
	lua_pushcfunction(L,lnng_dialer_set);
	lua_setfield(L,-2,"__newindex");

	luaL_newmetatable(L,"nng.dialer_m");
	luaL_newlib(L,nng_dialer_m);
	lua_setfield(L,-2,"__index");
	/*lua_pushcfunction(L,lnng_dialer_close);*/
	/*lua_setfield(L,-2,"__gc");*/
	lua_pop(L,1);

	luaL_newmetatable(L,"nng.listener");
	luaL_newlib(L,nng_listener_m);
	lua_setfield(L,-2,"__index");
	/*lua_pushcfunction(L,lnng_listener_close);*/
	/*lua_setfield(L,-2,"__gc");*/
	lua_pop(L,1);

	luaL_newmetatable(L,"nng.sockaddr");//{nng.sockaddr}
	lua_pushcfunction(L,lnng_sockaddr_get);//{nng.sockaddr},sockaddr_get
	lua_setfield(L,-2,"__index");//{nng_sockaddr}
	lua_pop(L,1);

	luaL_newlib(L,nng_f);
	luaopen_nng_aio(L);
	lua_setfield(L,-2,"aio");

	//Flags
	flag(NNG_FLAG_NONBLOCK);
	flag(NNG_FLAG_ALLOC);

	//Options
	option(NNG_OPT_SOCKNAME);
	option(NNG_OPT_SOCKNAME);
	option(NNG_OPT_RAW);
	option(NNG_OPT_PROTO);
	option(NNG_OPT_PROTONAME);
	option(NNG_OPT_PEER);
	option(NNG_OPT_PEERNAME);
	option(NNG_OPT_RECVBUF);
	option(NNG_OPT_SENDBUF);
	option(NNG_OPT_RECVFD);
	option(NNG_OPT_SENDFD);
	option(NNG_OPT_RECVTIMEO);
	option(NNG_OPT_SENDTIMEO);
	option(NNG_OPT_LOCADDR);
	option(NNG_OPT_REMADDR);
	option(NNG_OPT_URL);
	option(NNG_OPT_MAXTTL);
	option(NNG_OPT_RECVMAXSZ);
	option(NNG_OPT_RECONNMINT);
	option(NNG_OPT_RECONNMAXT);
	
	//TCP options
	option(NNG_OPT_TCP_NODELAY);
	option(NNG_OPT_TCP_KEEPALIVE);
	option(NNG_OPT_TCP_BOUND_PORT);

	//IPC options
	option(NNG_OPT_IPC_PEER_GID);
	option(NNG_OPT_IPC_PEER_PID);
	option(NNG_OPT_IPC_PEER_UID);
	option(NNG_OPT_IPC_PEER_ZONEID);
	option(NNG_OPT_IPC_PERMISSIONS);
	option(NNG_OPT_IPC_SECURITY_DESCRIPTOR);
	
	//Pub/sub options
	/*option(NNG_OPT_SUB_SUBSCRIBE);//should use socket:subscribe() instead (so that nil is not counted as part of the subscription)*/
	/*option(NNG_OPT_SUB_UNSUBSCRIBE);//should use socket:unsubscribe() instead (same reason as above)*/
	option(NNG_OPT_SUB_PREFNEW);

	//Req/rep options
	option(NNG_OPT_SURVEYOR_SURVEYTIME);

	return 1;
}
