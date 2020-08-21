CC?=gcc
CAFLAGS+=-I$(NNG_INCDIR) -I$(LUA_INCDIR) $(CFLAGS)
LADFLAGS+=$(LIBFLAG) $(LDFLAGS)
ifdef $(NNG_LIBDIR)
	LADFLAGS+= -L$(NNG_LIBDIR)
endif
ifdef $(LUA_LIBDIR)
	LADFLAGS+= -L$(LUA_LIBDIR)
endif
LD=gcc
LIBS=-lnng -llua53

ifeq ($(OS), Windows_NT)
	LDFLAGS+=-mwindows
	LIBS+=-lws2_32
else
endif


src_files=$(shell find src/*.c)
obj_files=$(src_files:src/%.c=build/%.o)
target=bin/nng.$(LIB_EXTENSION)

all: $(target)

$(target) : $(obj_files)
	$(LD) $(LADFLAGS) -o $@ $^ $(LIBS)

$(obj_files): build/%.o : src/%.c
	$(CC) $(CAFLAGS) -c -o $@ $<

install: $(target)
	$(CP) $(target) $(INST_LIBDIR)

test:
	busted --cpath=./bin/?$(SHARE_EXT)

clean:
	rm -rf build/*
	rm -rf bin/*
