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

struct Input {
	MBTiles mbtiles;
	std::vector<PreciseTileCoordinatesSet> zooms;
};
/**
 *\brief The Main function is responsible for command line processing, loading data and starting worker threads.
 *
 * Data is loaded into OsmMemTiles and ShpMemTiles.
 *
 * Worker threads write the output tiles, and start in the outputProc function.
 */
int main(const int argc, const char* argv[]) {
	std::vector<std::string> filenames;
	for (int i = 1; i < argc; i++) {
		filenames.push_back(std::string(argv[i]));
		std::cout << "arg " << std::to_string(i) << ": " << filenames.back() << std::endl;
	}

	if (filenames.empty()) {
		std::cerr << "usage: ./tile-smush file1.mbtiles file2mbtiles [...]" << std::endl;
		return 1;
	}

	std::vector<std::shared_ptr<Input>> inputs;
	for (auto filename : filenames) {
		std::shared_ptr<Input> input = std::make_shared<Input>();
		inputs.push_back(input);
		input->mbtiles.openForReading(filename);
		input->zooms.reserve(15);
		for (int zoom = 0; zoom < 15; zoom++)
			input->zooms.push_back(PreciseTileCoordinatesSet(zoom));

		// Determine which tiles exist in this mbtiles file.
		//
		// This lets us optimize the case where only a single mbtiles has
		// a tile for a given zxy, as we can copy the bytes directly.
		//
		// TODO: we should parallelize this
		input->mbtiles.populateTiles(input->zooms);

	}

	std::string MergedFilename("merged.mbtiles");
	remove(MergedFilename.c_str());

	MBTiles merged;
	merged.openForWriting(MergedFilename);


}

