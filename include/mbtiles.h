/*! \file */ 
#ifndef _MBTILES_H
#define _MBTILES_H

#include <string>
#include <vector>
#include "external/sqlite_modern_cpp.h"
#include "tile_coordinates_set.h"

struct PendingStatement {
	int zoom;
	int x;
	int y;
	std::string data;
	bool isMerge;
};

class Flock {
public:
	Flock(int fd);
	~Flock();

private:
	int fd_;
};

/** \brief Write to MBTiles (sqlite) database
*
* (note that sqlite_modern_cpp.h is very slightly changed from the original, for blob support and an .init method)
*/
class MBTiles { 
	sqlite::database db;
	std::vector<sqlite::database_binder> preparedStatements;
	int lockfd;
	bool inTransaction;
	std::string filename;

	std::shared_ptr<std::vector<PendingStatement>> pendingStatements1, pendingStatements2;

	void insertOrReplace(int zoom, int x, int y, const std::string& data, bool isMerge);
	void flushPendingStatements();

public:
	MBTiles();
	virtual ~MBTiles();
	void openForWriting(std::string &filename);
	void writeMetadata(std::string key, std::string value);
	std::vector<std::pair<std::string, std::string>> readMetadata();
	void saveTile(int zoom, int x, int y, std::string *data, bool isMerge);
	void closeForWriting();

	void populateTiles(bool verbose, std::vector<PreciseTileCoordinatesSet>& zooms, std::vector<Bbox>& extents);
	void openForReading(std::string &filename);
	void readBoundingBox(double &minLon, double &maxLon, double &minLat, double &maxLat);
	std::vector<char> readTile(int zoom, int col, int row);
};

#endif //_MBTILES_H

