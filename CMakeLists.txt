cmake_minimum_required(VERSION 3.18)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

project(tilesmush)

OPTION(TILESMUSH_BUILD_STATIC "Attempt to link dependencies static" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
  message(FATAL_ERROR "In-source builds are not allowed, please use a separate build directory.")
endif()

if (POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif()

IF (TILESMUSH_BUILD_STATIC)
	SET (CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_STATIC_LIBRARY_SUFFIX})
ENDIF ()

find_package(ZLIB REQUIRED)

set(CMAKE_CXX_STANDARD 17)

if(!TM_VERSION)
	execute_process(
		COMMAND git describe --tags --abbrev=0
		OUTPUT_VARIABLE tm_version)
	add_compile_definitions(TM_VERSION=${tm_version})
endif()

if(MSVC)
 	find_package(unofficial-sqlite3 CONFIG REQUIRED)
	add_library(SQLite::SQLite3 ALIAS unofficial::sqlite3::sqlite3)
	add_definitions(-D_USE_MATH_DEFINES -DWIN32_LEAN_AND_MEAN -DNOGDI -D__restrict__=__restrict)
  	set(THREAD_LIB "")
else()
	find_package(SQLite3 REQUIRED)
  	set(THREAD_LIB pthread)
endif()

file(GLOB tilesmush_src_files
	src/coordinates.cpp
	src/helpers.cpp
	src/mbtiles.cpp
	src/tile_coordinates_set.cpp
	src/tile-smush.cpp
  )
add_executable(tile-smush ${tilesmush_src_files})
target_include_directories(tile-smush PRIVATE include)
target_include_directories(tile-smush PRIVATE ${CMAKE_BINARY_DIR}) # for generated files
target_link_libraries(tile-smush
		${THREAD_LIB} ${CMAKE_DL_LIBS}
		SQLite::SQLite3
		ZLIB::ZLIB
		)

include(CheckCxxAtomic)
if(NOT HAVE_CXX11_ATOMIC)
	string(APPEND CMAKE_CXX_STANDARD_LIBRARIES
	" ${LIBATOMIC_LINK_FLAGS}")
endif()

install(TARGETS tile-smush RUNTIME DESTINATION bin)
