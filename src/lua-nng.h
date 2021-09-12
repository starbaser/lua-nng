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

nng_socket* tosocket(lua_State *L, int offset);
nng_listener* tolistener(lua_State *L, int offset);
nng_dialer* todialer(lua_State *L, int offset);
