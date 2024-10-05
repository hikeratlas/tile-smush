# See what Lua versions are installed
# order of preference: LuaJIT, any generic Lua, then versions from 5.4 down

PLATFORM_PATH := /usr/local

# First, find what the Lua executable is called
# - when a new Lua is released, then add it before 5.4 here
LUA_CMD := $(shell luajit -e 'print("luajit")' 2> /dev/null || lua -e 'print("lua")' 2> /dev/null || lua5.4 -e 'print("lua5.4")' 2> /dev/null || lua5.3 -e 'print("lua5.3")' 2> /dev/null || lua5.2 -e 'print("lua5.2")' 2> /dev/null || lua5.1 -e 'print("lua5.1")' 2> /dev/null)
ifeq ($(LUA_CMD),"")
  $(error Couldn't find Lua interpreter)
endif
$(info Using ${LUA_CMD})

# Find the language version
LUA_LANGV := $(shell ${LUA_CMD} -e 'print(string.match(_VERSION, "%d+.%d+"))')
$(info - Lua language version ${LUA_LANGV})

# Find the directory where Lua might be
ifeq ($(LUA_CMD),luajit)
  # We need the LuaJIT version (2.0/2.1) to find this
  LUA_JITV := $(shell luajit -e 'a,b,c=string.find(jit.version,"LuaJIT (%d.%d)");print(c)')
  $(info - LuaJIT version ${LUA_JITV})
  LUA_DIR := luajit-${LUA_JITV}
  LUA_LIBS := -lluajit-${LUA_LANGV}
else
  LUA_DIR := $(LUA_CMD)
  LUA_LIBS := -l${LUA_CMD}
endif

# Find the include path by looking in the most likely locations
ifneq ('$(wildcard /usr/local/include/${LUA_DIR}/lua.h)','')
  LUA_CFLAGS := -I/usr/local/include/${LUA_DIR}
else ifneq ('$(wildcard /usr/local/include/${LUA_DIR}${LUA_LANGV}/lua.h)','')
  LUA_CFLAGS := -I/usr/local/include/${LUA_DIR}${LUA_LANGV}
  LUA_LIBS := -l${LUA_CMD}${LUA_LANGV}
else ifneq ('$(wildcard /usr/include/${LUA_DIR}/lua.h)','')
  LUA_CFLAGS := -I/usr/include/${LUA_DIR}
else ifneq ('$(wildcard /usr/include/${LUA_DIR}${LUA_LANGV}/lua.h)','')
  LUA_CFLAGS := -I/usr/include/${LUA_DIR}${LUA_LANGV}
  LUA_LIBS := -l${LUA_CMD}${LUA_LANGV}
else ifneq ('$(wildcard /usr/include/lua.h)','')
  LUA_CFLAGS := -I/usr/include
else ifneq ('$(wildcard /opt/homebrew/include/${LUA_DIR}/lua.h)','')
  LUA_CFLAGS := -I/opt/homebrew/include/${LUA_DIR}
  PLATFORM_PATH := /opt/homebrew
else ifneq ('$(wildcard /opt/homebrew/include/${LUA_DIR}${LUA_LANGV}/lua.h)','')
  LUA_CFLAGS := -I/opt/homebrew/include/${LUA_DIR}${LUA_LANGV}
  LUA_LIBS := -l${LUA_CMD}${LUA_LANGV}
  PLATFORM_PATH := /opt/homebrew
else
  $(error Couldn't find Lua libraries)
endif

# Append LuaJIT-specific flags if needed
ifeq ($(LUA_CMD),luajit)
  LUA_CFLAGS := ${LUA_CFLAGS} -DLUAJIT
  ifneq ($(OS),Windows_NT)
    ifeq ($(shell uname -s), Darwin)
      ifeq ($(LUA_JITV),2.0)
        LDFLAGS := -pagezero_size 10000 -image_base 100000000
        $(info - with MacOS LuaJIT linking)
      endif
    endif
  endif
endif

# Report success
$(info - include path is ${LUA_CFLAGS})
$(info - library path is ${LUA_LIBS})

# Main includes

prefix = /usr/local

MANPREFIX := /usr/share/man
TM_VERSION ?= $(shell git describe --tags --abbrev=0)
CXXFLAGS ?= -g -O3 -Wall -Wno-unknown-pragmas -Wno-sign-compare -std=c++14 -pthread -fPIE -DTM_VERSION=$(TM_VERSION) $(CONFIG)
CFLAGS ?= -g -O3 -Wall -Wno-unknown-pragmas -Wno-sign-compare -std=c99 -fPIE -DTM_VERSION=$(TM_VERSION) $(CONFIG)
LIB := -L$(PLATFORM_PATH)/lib -lz $(LUA_LIBS) -lboost_program_options -lsqlite3 -lboost_filesystem -lboost_system -lboost_iostreams -lshp -pthread
INC := -I$(PLATFORM_PATH)/include -isystem ./include -I./src $(LUA_CFLAGS)

# Targets
.PHONY: test

all: tilesmush

tilesmush: \
	src/coordinates.o \
	src/helpers.o \
	src/mbtiles.o \
	src/tile_coordinates_set.o \
	src/tilemaker.o
	$(CXX) $(CXXFLAGS) -o tile-smush $^ $(INC) $(LIB) $(LDFLAGS)

test: \
	test_helpers

test_helpers: \
	src/helpers.o \
	test/helpers.test.o
	$(CXX) $(CXXFLAGS) -o test.helpers $^ $(INC) $(LIB) $(LDFLAGS) && ./test.helpers

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ -c $< $(INC)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $< $(INC)

install:
	install -m 0755 -d $(DESTDIR)$(prefix)/bin/
	install -m 0755 tile-smush $(DESTDIR)$(prefix)/bin/

clean:
	rm -f tile-smush src/*.o src/external/*.o include/*.o include/*.pb.h server/*.o test/*.o rm test.*

.PHONY: install
