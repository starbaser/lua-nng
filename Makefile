CC?=gcc
CAFLAGS+=-I$(NNG_INCDIR) -I$(LUA_INCDIR) -L$(LUA_LIBDIR) $(CFLAGS)
LADFLAGS+=$(LIBFLAG) $(LDFLAGS)
LIBS=-lnng
ifdef NNG_LIBDIR
	LADFLAGS+= -L$(NNG_LIBDIR)
endif
ifdef LUA_LIBDIR
	LADFLAGS+= -L$(LUA_LIBDIR)
endif
ifdef LUA_LIB
	LIBS+= -l$(LUA_LIB)
endif
LD=gcc

ifeq ($(OS), Windows_NT)
	LDFLAGS+=-mwindows
	LIBS+=-lws2_32
else
endif


src_files=$(wildcard src/*.c)
obj_files=$(src_files:src/%.c=build/%.o)
target=bin/nng.$(LIB_EXTENSION)
installed_target=$(target:bin/%=$(INST_LIBDIR)/%)

.PHONY: all install test clean

all: $(target)

$(target) : $(obj_files)
	$(LD) $(LADFLAGS) -o $@ $^ $(LIBS)

$(installed_target) : $(target)
	$(MKDIR) -p $(@D)
	$(CP) $< $@

$(obj_files): build/%.o : src/%.c
	$(CC) $(CAFLAGS) -c -o $@ $<

install: $(installed_target)

test:
	busted --cpath=./bin/?$(SHARE_EXT)

clean:
	rm -rf build/*
	rm -rf bin/*
