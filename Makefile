PLATFORM_PATH := /usr/local

# Main includes

prefix = /usr/local

MANPREFIX := /usr/share/man
TM_VERSION ?= $(shell git describe --tags --abbrev=0)
CXXFLAGS ?= -g -O3 -Wall -Wno-unknown-pragmas -Wno-sign-compare -std=c++14 -pthread -fPIE -DTM_VERSION=$(TM_VERSION) $(CONFIG)
CFLAGS ?= -g -O3 -Wall -Wno-unknown-pragmas -Wno-sign-compare -std=c99 -fPIE -DTM_VERSION=$(TM_VERSION) $(CONFIG)
LIB := -L$(PLATFORM_PATH)/lib -lz -lsqlite3 -pthread
INC := -I$(PLATFORM_PATH)/include -isystem ./include -I./src

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
