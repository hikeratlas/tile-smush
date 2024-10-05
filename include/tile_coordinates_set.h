#ifndef TILE_COORDINATES_SET_H
#define TILE_COORDINATES_SET_H

#include <cstddef>
#include "coordinates.h"
#include <vector>

struct Bbox {
	size_t minX;
	size_t minY;
	size_t maxX;
	size_t maxY;
};

// Read-write implementation for precise sets; maximum zoom is
// generally expected to be z14.
class PreciseTileCoordinatesSet {
public:
	PreciseTileCoordinatesSet(unsigned int zoom);
	bool test(TileCoordinate x, TileCoordinate y) const;
	size_t size() const;
	size_t zoom() const;
	void set(TileCoordinate x, TileCoordinate y);

private:
	unsigned int zoom_;
	std::vector<bool> tiles;
};
#endif
