PLATFORM_PATH := /usr/local

# Main includes

prefix = /usr/local

MANPREFIX := /usr/share/man
TM_VERSION ?= $(shell git describe --tags --abbrev=0)
CXXFLAGS ?= -g -O3 -Wall -Wno-unknown-pragmas -Wno-sign-compare -std=c++14 -pthread -fPIE -DTM_VERSION=$(TM_VERSION) $(CONFIG)
CFLAGS ?= -g -O3 -Wall -Wno-unknown-pragmas -Wno-sign-compare -std=c99 -fPIE -DTM_VERSION=$(TM_VERSION) $(CONFIG)
LIB := -L$(PLATFORM_PATH)/lib -lsqlite3 -pthread
INC := -I$(PLATFORM_PATH)/include -isystem ./include -I./src

# Targets
.PHONY: test

all: tilesmush

tilesmush: \
	src/coordinates.o \
	src/external/libdeflate/lib/adler32.o \
	src/external/libdeflate/lib/arm/cpu_features.o \
	src/external/libdeflate/lib/crc32.o \
	src/external/libdeflate/lib/deflate_compress.o \
	src/external/libdeflate/lib/deflate_decompress.o \
	src/external/libdeflate/lib/gzip_compress.o \
	src/external/libdeflate/lib/gzip_decompress.o \
	src/external/libdeflate/lib/utils.o \
	src/external/libdeflate/lib/x86/cpu_features.o \
	src/external/libdeflate/lib/zlib_compress.o \
	src/external/libdeflate/lib/zlib_decompress.o \
	src/helpers.o \
	src/mbtiles.o \
	src/tile_coordinates_set.o \
	src/tile-smush.o
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
