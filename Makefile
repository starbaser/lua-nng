CC=gcc
LIBS+=-lnng
ALL_CFLAGS=$(CFLAGS)
ALL_LDFLAGS=$(LDFLAGS)
ifdef NNG_LIBDIR
	ALL_LDFLAGS+= -L$(NNG_LIBDIR)
endif
ifdef LUA_LIBDIR
	ALL_LDFLAGS+= -L$(LUA_LIBDIR)
endif
ifdef LUA_LIB
	LIBS+= -l$(LUA_LIB)
endif
ifdef NNG_INCDIR
	ALL_CFLAGS+= -I$(NNG_INCDIR)
endif
ifdef LUA_INCDIR
	ALL_CFLAGS+= -I$(LUA_INCDIR)
endif
ifndef CC
	CC=gcc
endif
ifeq ($(OS), Windows_NT)
	ALL_LDFLAGS+=-mwindows
	LIBS+=-lws2_32
endif


src_files=$(wildcard src/*.c)
obj_files=$(src_files:src/%.c=build/%.o)
target=bin/nng.$(LIB_EXTENSION)
installed_target=$(target:bin/%=$(INST_LIBDIR)/%)

.PHONY: all install test clean

all: $(target)

$(target) : $(obj_files)
	$(CC) $(ALL_LDFLAGS) -o $@ $^ $(LIBS)

$(installed_target) : $(target)
	$(MKDIR) -p $(@D)
	$(CP) $< $@

$(obj_files): build/%.o : src/%.c
	$(CC) $(ALL_CFLAGS) -c -o $@ $<

install: $(installed_target)

test:
	busted --cpath=./bin/?$(SHARE_EXT)

clean:
	rm -rf build/*
	rm -rf bin/*
