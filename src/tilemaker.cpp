/*! \file */ 

// C++ includes
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <cmath>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <chrono>

// Other utilities
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <boost/variant.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/sort/sort.hpp>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"

#ifndef _MSC_VER
#include <sys/resource.h>
#endif

#include "geom.h"

// Tilemaker code
#include "helpers.h"
#include "coordinates.h"
#include "coordinates_geom.h"
#include "tile_coordinates_set.h"

#include "mbtiles.h"

#include <boost/asio/post.hpp>
#include <boost/interprocess/streams/bufferstream.hpp>

#ifndef TM_VERSION
#define TM_VERSION (version not set)
#endif
#define STR1(x)  #x
#define STR(x)  STR1(x)

// Namespaces
using namespace std;
namespace po = boost::program_options;
namespace geom = boost::geometry;

// Global verbose switch
bool verbose = false;


/**
 *\brief The Main function is responsible for command line processing, loading data and starting worker threads.
 *
 * Data is loaded into OsmMemTiles and ShpMemTiles.
 *
 * Worker threads write the output tiles, and start in the outputProc function.
 */
int main(const int argc, const char* argv[]) {
}

