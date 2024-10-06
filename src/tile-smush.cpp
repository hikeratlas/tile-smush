/*! \file */ 

// C++ includes
#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <cmath>
#include <stdexcept>
#include <thread>
#include <deque>
#include <map>

// Tilemaker code
#include "helpers.h"
#include "tile_coordinates_set.h"
#include "mbtiles.h"

#include <vtzero/builder.hpp>

#ifndef TM_VERSION
#define TM_VERSION (version not set)
#endif
#define STR1(x)  #x
#define STR(x)  STR1(x)

// Namespaces
using namespace std;

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
	uint64_t shards = 1;
	uint64_t shard = 0;

	if (getenv("SHARDS") != NULL) {
		shards = atoi(getenv("SHARDS"));
	}

	if (getenv("SHARD") != NULL) {
		shard = atoi(getenv("SHARD"));
	}

	std::cout << "shards=" << std::to_string(shards) << " shard=" << std::to_string(shard) << std::endl;

	if (shard >= shards) {
		std::cerr << "fatal: shard must be less than shards" << std::endl;
		return 1;
	}


	std::vector<std::string> filenames;
	for (int i = 1; i < argc; i++) {
		filenames.push_back(std::string(argv[i]));
		if (false && shard == 0)
			std::cout << "arg " << std::to_string(i) << ": " << filenames.back() << std::endl;
	}

	if (filenames.empty()) {
		if (shard == 0)
			std::cerr << "usage: ./tile-smush file1.mbtiles file2.mbtiles [...]" << std::endl;
		return 1;
	}

	// See https://github.com/xerial/sqlite-jdbc/issues/59#issuecomment-162115704
	int rv;
	rv = sqlite3_config(SQLITE_CONFIG_MEMSTATUS, 0);
	if (rv) {
		std::cerr << "fatal: sqlite3_config(SQLITE_CONFIG_MEMSTATUS)=" << std::to_string(rv) << std::endl;
		return 1;
	}

	// We only use a single thread, so tell SQLite to skip its internal locking.
	// See https://www.sqlite.org/c3ref/c_config_covering_index_scan.html#sqliteconfigsinglethread
	rv = sqlite3_config(SQLITE_CONFIG_SINGLETHREAD);
	if (rv) {
		std::cerr << "fatal: sqlite3_config(SQLITE_CONFIG_SINGLETHREAD)=" << std::to_string(rv) << std::endl;
		return 1;
	}

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

		input->mbtiles.populateTiles(shard == 0, input->zooms, input->bbox);
	}

	std::string MergedFilename("merged.mbtiles");

	if (shards == 1) {
		// When we're running on the entire dataset, remove the merged.mbtiles file.
		// Otherwise, we rely on tile-smush-parallel to do this.
		remove(MergedFilename.c_str());
	}

	MBTiles merged;
	merged.openForWriting(MergedFilename);

	if (shard == 0) {
		// Populate the `metadata` table
		// See https://github.com/mapbox/mbtiles-spec/blob/master/1.3/spec.md#content

		double minLon = std::numeric_limits<double>::max(),
					 maxLon = std::numeric_limits<double>::min(),
					 minLat = std::numeric_limits<double>::max(),
					 maxLat = std::numeric_limits<double>::min();
		int minzoom = 100;
		int maxzoom = 0;
		double minLonCurrent, maxLonCurrent, minLatCurrent, maxLatCurrent;

		std::map<std::string, std::string> metadata;

		// Use a map to dedupe. You can have _the exact same_ layer, e.g.
		// as hikeratlas does with its hacky parks/city_parks hijinks.
		std::map<std::string, std::string> layers;
		for (auto& input : inputs) {
			for (const auto& entry : input->mbtiles.readMetadata()) {
				metadata[entry.first] = entry.second;

				if (entry.first == "minzoom" || entry.first == "maxzoom") {
					int zoom = atoi(entry.second.c_str());

					if (entry.first == "minzoom" && zoom < minzoom)
						minzoom = zoom;
					if (entry.first == "maxzoom" && zoom > maxzoom)
						maxzoom = zoom;
				}

				if (entry.first == "json") {
					// This is incredibly hacky! I don't want to learn how to use a C++ JSON
					// library
					const char* vectorLayers = strstr(entry.second.c_str(), "\"vector_layers\":[");
					if (!vectorLayers) {
						throw std::runtime_error("no vector_layers found for " + input->filename);
					}

					vectorLayers += strlen("\"vector_layers\":[");
					//std::cout << "INPUT: " << vectorLayers << std::endl;

					const char* start = NULL;
					// This is a total hack, it'll fail if you have braces in strings, e.g.
					int braces = 0;
					while(*vectorLayers != ']') {
						if (start == NULL && *vectorLayers == ']')
							break;

						if (start == NULL && *vectorLayers == '{') {
							start = vectorLayers;
						}

						if (*vectorLayers == '{') {
							braces++;
						}

						if (*vectorLayers == '}') {
							braces--;
						}

						if (start && braces == 0) {
							std::string layer(start, vectorLayers - start + 1);
							//std::cout << "LAYER: " << layer << std::endl;

							layers[layer] = "";
							start = NULL;
						}

						vectorLayers++;
					}
				}
			}

			input->mbtiles.readBoundingBox(minLonCurrent, maxLonCurrent, minLatCurrent, maxLatCurrent);

			if (minLonCurrent < minLon) minLon = minLonCurrent;
			if (minLatCurrent < minLat) minLat = minLatCurrent;
			if (maxLonCurrent > maxLon) maxLon = maxLonCurrent;
			if (maxLatCurrent > maxLat) maxLat = maxLatCurrent;
		}

		// Dump the metadata into merged.mbtiles
		for (auto const& entry : metadata) {
			merged.writeMetadata(entry.first, entry.second);
		}

		merged.writeMetadata(
			"bounds", 
			std::to_string(minLon) + "," +
			std::to_string(minLat) + "," +
			std::to_string(maxLon) + "," +
			std::to_string(maxLat)
		);

		merged.writeMetadata("minzoom", std::to_string(minzoom));
		merged.writeMetadata("maxzoom", std::to_string(maxzoom));

		std::string vector_layers = "{\"vector_layers\":[";
		int i = 0;
		for (auto const& entry : layers) {
			if (i > 0)
				vector_layers += ",";

			vector_layers += entry.first;
			i++;
		}

		vector_layers += "]}";
		merged.writeMetadata("json", vector_layers);
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

		// std::cout << "z=" << std::to_string(zoom) << " minX=" << std::to_string(bbox.minX) << " minY=" << std::to_string(bbox.minY) << " maxX=" << std::to_string(bbox.maxX) << " maxY=" << std::to_string(bbox.maxY) << std::endl;

		for (int x = bbox.minX; x <= bbox.maxX; x++) {
			for (int y = bbox.minY; y <= bbox.maxY; y++) {
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
					std::vector<char> old = matching[0]->mbtiles.readTile(zoom, x, y);
					std::string buffer(old.data(), old.size());
					merged.saveTile(zoom, x, y, &buffer, false);
					continue;
				}

				//std::cout << "need to merge z=" << std::to_string(zoom) << " x=" << std::to_string(x) << " y=" << std::to_string(y) << std::endl;
				// Multiple mbtiles want to contribute a tile at this zxy.
				// They'll all have disjoint layers, so decompress each tile
				// and concatenate their contents to form the new tile.

				vtzero::tile_builder builder;

				std::deque<std::string> strs;
				for (auto& match : matching) {
					std::vector<char> compressed = match->mbtiles.readTile(zoom, x, y);

					std::string oldTile;
					decompress_string(oldTile, compressed.data(), compressed.size(), true);
					//std::cout << "compressed.size()=" << std::to_string(compressed.size()) << " oldTile.size()=" << std::to_string(oldTile.size()) << std::endl;

					strs.push_back(oldTile);
					vtzero::vector_tile existingTile{strs.back()};
					while (auto layer = existingTile.next_layer()) {
						builder.add_existing_layer(layer);
						std::string layerName(layer.name().data(), layer.name().size());
						//std::cout << "adding layer=" << layerName << " features=" << std::to_string(layer.num_features()) << std::endl;
					}
				}

				std::string buffer;
				builder.serialize(buffer);
				std::string compressed = compress_string(buffer, 6, true);
				merged.saveTile(zoom, x, y, &compressed, false);
			}
		}
	}

	merged.closeForWriting();

}

