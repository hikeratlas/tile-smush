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

thread_local std::vector<std::shared_ptr<MBTiles>> tlsTiles;

struct Input {
	uint16_t index;
	std::string filename;
	MBTiles mbtiles;
	std::vector<PreciseTileCoordinatesSet> zooms;
	std::vector<Bbox> bbox;
};
/**
 *\brief The Main function is responsible for command line processing, loading data and starting worker threads.
 *
 * Data is loaded into OsmMemTiles and ShpMemTiles.
 *
 * Worker threads write the output tiles, and start in the outputProc function.
 */
int main(const int argc, const char* argv[]) {
	uint16_t threadNum = max(thread::hardware_concurrency(), 1u);
	std::vector<std::string> filenames;
	for (int i = 1; i < argc; i++) {
		filenames.push_back(std::string(argv[i]));
		std::cout << "arg " << std::to_string(i) << ": " << filenames.back() << std::endl;
	}

	if (filenames.empty()) {
		std::cerr << "usage: ./tile-smush file1.mbtiles file2.mbtiles [...]" << std::endl;
		return 1;
	}

	{
		// See https://github.com/xerial/sqlite-jdbc/issues/59#issuecomment-162115704
		int rv;
		rv = sqlite3_config(SQLITE_CONFIG_MEMSTATUS, 0);
		if (rv) {
			std::cerr << "fatal: sqlite3_config(SQLITE_CONFIG_MEMSTATUS)=" << std::to_string(rv) << std::endl;
			return 1;
		}

		// Per https://www.sqlite.org/c3ref/c_config_covering_index_scan.html#sqliteconfigsinglethread,
		// this should break everything.
		//
		// But things seem to still work, and this avoids the locking we were seeing.
		//
		// I would have thought we needed SQLITE_CONFIG_MULTITHREAD ? I feel like
		// I'm missing something pretty big here, but let's go until we hit a wall.
		rv = sqlite3_config(SQLITE_CONFIG_SINGLETHREAD);
		if (rv) {
			std::cerr << "fatal: sqlite3_config(SQLITE_CONFIG_SINGLETHREAD)=" << std::to_string(rv) << std::endl;
			return 1;
		}
	}
	std::string MergedFilename("merged.mbtiles");
	remove(MergedFilename.c_str());

	MBTiles merged;
	merged.openForWriting(MergedFilename);

	std::vector<std::shared_ptr<Input>> inputs;
	for (auto filename : filenames) {
		std::shared_ptr<Input> input = std::make_shared<Input>();
		input->filename = filename;
		input->index = inputs.size();
		inputs.push_back(input);
		input->mbtiles.openForReading(filename);
		input->zooms.reserve(15);
		for (int zoom = 0; zoom < 15; zoom++) {
			input->zooms.push_back(PreciseTileCoordinatesSet(zoom));
			input->bbox.push_back({
				std::numeric_limits<size_t>::max(),
				std::numeric_limits<size_t>::max(),
				std::numeric_limits<size_t>::min(),
				std::numeric_limits<size_t>::min()
			});
		}
	}

	{
		boost::asio::thread_pool pool(threadNum);

		for (auto& input : inputs) {
			// TODO: this segfaults when run in the thread pool, which _is_
			// what I expect SQLITE_CONFIG_SINGLETHREAD to do...
			//
			// ...but why does this die, and our very similar use immediately
			// below does not?
			//boost::asio::post(pool, [&]() {
				// Determine which tiles exist in this mbtiles file.
				//
				// This lets us optimize the case where only a single mbtiles has
				// a tile for a given zxy, as we can copy the bytes directly.
				//
				//MBTiles mbtiles;
				//mbtiles.openForReading(input->filename);
				//mbtiles.populateTiles(input->zooms, input->bbox);
				input->mbtiles.populateTiles(input->zooms, input->bbox);
			//});
		}

		pool.join();
	}

	{
		boost::asio::thread_pool pool(threadNum);

		const uint64_t shards = threadNum * 2;
		for (uint64_t shard = 0; shard < shards; shard++) {
			boost::asio::post(pool, [&, shard]() {
				// Ensure we have tls copies of each of the mbtiles

				if (tlsTiles.empty()) {
					for (const auto& input : inputs) {
						tlsTiles.push_back(std::make_shared<MBTiles>());
						tlsTiles.back()->openForReading(input->filename);
					}
				}

				std::vector<Input*> matching;
				for (int zoom = 0; zoom < 15; zoom++) {
					Bbox bbox = inputs[0]->bbox[zoom];
					for (const auto& input : inputs) {
						if (input->bbox[zoom].minX < bbox.minX) bbox.minX = input->bbox[zoom].minX;
						if (input->bbox[zoom].minY < bbox.minY) bbox.minY = input->bbox[zoom].minY;
						if (input->bbox[zoom].maxX > bbox.maxX) bbox.maxX = input->bbox[zoom].maxX;
						if (input->bbox[zoom].maxY > bbox.maxY) bbox.maxY = input->bbox[zoom].maxY;
					}

					for (int x = bbox.minX; x < bbox.maxX; x++) {
						for (int y = bbox.minY; y < bbox.maxY; y++) {
							if ((x * (1 << zoom) + y) % shards != shard)
								continue;

							matching.clear();
							for (const auto& input : inputs) {
								if (input->zooms[zoom].test(x, y))
									matching.push_back(input.get());
							}

							if (matching.empty())
								continue;

							if (matching.size() == 1) {
								// When exactly 1 mbtiles matches, it's a special case and we can
								// copy directly between them.
								std::vector<char> old = tlsTiles[matching[0]->index]->readTile(zoom, x, y);
								//std::vector<char> old = matching[0]->mbtiles.readTile(zoom, x, y);

								std::string buffer(old.data(), old.size());
								merged.saveTile(zoom, x, y, &buffer, false);
								continue;
							}

							//std::cout << "z=" << std::to_string(zoom) << " x=" << std::to_string(x) << " y=" << std::to_string(y) << " has " << std::to_string(matching.size()) << " tiles" << std::endl;
						}
					}
				}
			});
		}
		
		pool.join();
	}

	// TODO: Populate the `metadata` table
	// See https://github.com/mapbox/mbtiles-spec/blob/master/1.3/spec.md#content

	merged.closeForWriting();

}

