#include "tile_coordinates_set.h"
#include <string>
#include <stdexcept>
#include <iostream>

PreciseTileCoordinatesSet::PreciseTileCoordinatesSet(unsigned int zoom):
	zoom_(zoom),
	tiles((1 << zoom) * (1 << zoom)) {}

bool PreciseTileCoordinatesSet::test(TileCoordinate x, TileCoordinate y) const {
	uint64_t loc = x * (1 << zoom_) + y;
	if (loc >= tiles.size())
		return false;

	return tiles[loc];
}

size_t PreciseTileCoordinatesSet::zoom() const {
	return zoom_;
}

size_t PreciseTileCoordinatesSet::size() const {
	size_t rv = 0;
	for (int i = 0; i < tiles.size(); i++)
		if (tiles[i])
			rv++;

	return rv;
}

void PreciseTileCoordinatesSet::set(TileCoordinate x, TileCoordinate y) {
	uint64_t loc = x * (1 << zoom_) + y;
	if (loc >= tiles.size())
		return;
	tiles[loc] = true;
}


