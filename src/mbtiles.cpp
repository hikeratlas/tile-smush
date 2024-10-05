#include "mbtiles.h"
#include "helpers.h"
#include <iostream>
#include <cmath>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/device/array.hpp>
#include <fcntl.h>
#include <sys/file.h>

using namespace sqlite;
using namespace std;
namespace bio = boost::iostreams;

Flock::Flock(int fd) {
	fd_ = 0;

	int rv = flock(fd, LOCK_EX);
	if (rv == 0)
		fd_ = fd;
	else
		throw std::runtime_error("failed to flock");
}

Flock::~Flock() {
	if (fd_)
		flock(fd_, LOCK_UN);
}

MBTiles::MBTiles():
	inTransaction(false),
  pendingStatements1(std::make_shared<std::vector<PendingStatement>>()),
  pendingStatements2(std::make_shared<std::vector<PendingStatement>>())
{
	lockfd = 0;
	lockfd = open("./lockfile", O_CREAT, 0644);
	if (lockfd == -1)
		throw std::runtime_error("failed to open lockfile");

	//std::cout << "lockfd=" << std::to_string(lockfd) << std::endl;
}

MBTiles::~MBTiles() {
	{
		Flock lock(lockfd);
		if (db && inTransaction) {
			db << "COMMIT;"; // commit all the changes if open
		}

		// Reset the DB member so that the sqlite3_close_v2() command is called
		// inside of the flock
		(void)[v=std::move(db)]{};
	}

	if (lockfd) {
		close(lockfd);
	}
}

// ---- Write .mbtiles

void MBTiles::openForWriting(string &filename) {
	Flock lock(lockfd);
	db.init(filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
	this->filename = filename;

	db << "PRAGMA synchronous = OFF;";
	try {
		db << "PRAGMA application_id = 0x4d504258;";
	} catch(runtime_error &e) {
		cout << "Couldn't write SQLite application_id (not fatal): " << e.what() << endl;
	}
	try {
		db << "PRAGMA encoding = 'UTF-8';";
	} catch(runtime_error &e) {
		cout << "Couldn't set SQLite default encoding (not fatal): " << e.what() << endl;
	}
	try {
		db << "PRAGMA journal_mode=WAL;";
	} catch(runtime_error &e) {
		cout << "Couldn't turn journaling on (not fatal): " << e.what() << endl;
	}
	db << "PRAGMA page_size = 65536;";
	db << "VACUUM;"; // make sure page_size takes effect
	db << "CREATE TABLE IF NOT EXISTS metadata (name text, value text, UNIQUE (name));";
	db << "CREATE TABLE IF NOT EXISTS tiles (zoom_level integer, tile_column integer, tile_row integer, tile_data blob);";
	db << "CREATE UNIQUE INDEX IF NOT EXISTS tile_index on tiles (zoom_level, tile_column, tile_row);";
	preparedStatements.emplace_back(db << "INSERT INTO tiles (zoom_level, tile_column, tile_row, tile_data) VALUES (?,?,?,?);");
	preparedStatements.emplace_back(db << "REPLACE INTO tiles (zoom_level, tile_column, tile_row, tile_data) VALUES (?,?,?,?);");

	//cout << "Creating mbtiles at " << filename << endl;
//	db << "BEGIN;"; // begin a transaction
//	inTransaction = true;
}
	
void MBTiles::writeMetadata(string key, string value) {
	Flock lock(lockfd);
	db << "REPLACE INTO metadata (name,value) VALUES (?,?);" << key << value;
}

std::vector<std::pair<std::string, std::string>> MBTiles::readMetadata() {
	Flock lock(lockfd);

	std::vector<std::pair<std::string, std::string>> rv;
	db << "SELECT name, value FROM metadata;" >> [&](std::string name, std::string value) {
		rv.push_back(std::make_pair(name, value));
	};

	return rv;
}

void MBTiles::insertOrReplace(int zoom, int x, int y, const std::string& data, bool isMerge) {
	// NB: assumes we have n flock on lockfd
	int tmsY = pow(2, zoom) - 1 - y;
	int s = isMerge ? 1 : 0;
	preparedStatements[s].reset();
	preparedStatements[s] << zoom << x << tmsY && data;
	preparedStatements[s].execute();
}

void MBTiles::flushPendingStatements() {
	Flock lock(lockfd);

	db << "BEGIN";

	for (int i = 0; i < 2; i++) {
		while(!pendingStatements2->empty()) {
			const PendingStatement& stmt = pendingStatements2->back();
			insertOrReplace(stmt.zoom, stmt.x, stmt.y, stmt.data, stmt.isMerge);
			pendingStatements2->pop_back();
		}

		std::lock_guard<std::mutex> lock(pendingStatementsMutex);
		pendingStatements1.swap(pendingStatements2);
	}

	db << "COMMIT";
}
	
void MBTiles::saveTile(int zoom, int x, int y, string *data, bool isMerge) {
	//std::cerr << "writing zoom=" << std::to_string(zoom) << " x=" << std::to_string(x) << " y=" << std::to_string(y) << std::endl;
	pendingStatements1->push_back({zoom, x, y, *data, isMerge});

	if (pendingStatements1->size() > 10000)
		flushPendingStatements();
}

void MBTiles::populateTiles(bool verbose, std::vector<PreciseTileCoordinatesSet>& zooms, std::vector<Bbox>& extents) {
	size_t tiles = 0;
	db << "SELECT zoom_level,tile_column,tile_row FROM tiles" >> [&](int z,int col, int row) {
		tiles++;
		zooms[z].set(col, row);

		if (col > extents[z].maxX) extents[z].maxX = col;
		if (col < extents[z].minX) extents[z].minX = col;
		if (row > extents[z].maxY) extents[z].maxY = row;
		if (row < extents[z].minY) extents[z].minY = row;
	};
	if (verbose)
		std::cout << filename << " had " << std::to_string(tiles) << " tiles" << std::endl;
}

void MBTiles::closeForWriting() {
	Flock lock(lockfd);
	flushPendingStatements();
	preparedStatements[0].used(true);
	preparedStatements[1].used(true);
}

// ---- Read mbtiles

void MBTiles::openForReading(string &filename) {
	std::string uri = "file:";
	uri += filename;
	uri += "?immutable=1&mode=ro";
	//db.init(uri.c_str(), SQLITE_OPEN_READONLY | SQLITE_OPEN_URI | SQLITE_OPEN_NOMUTEX);
	db.init(uri.c_str(), SQLITE_OPEN_READONLY | SQLITE_OPEN_URI);
	this->filename = filename;

	/*
	db << "pragma compile_options" >> [&](string str) {
		std::cout << str << std::endl;
	};
	*/
}

void MBTiles::readBoundingBox(double &minLon, double &maxLon, double &minLat, double &maxLat) {
	string boundsStr;
	db << "SELECT value FROM metadata WHERE name='bounds'" >> boundsStr;
	vector<string> b = split_string(boundsStr,',');
	minLon = stod(b[0]); minLat = stod(b[1]);
	maxLon = stod(b[2]); maxLat = stod(b[3]);
}

vector<char> MBTiles::readTile(int zoom, int col, int row) {
	vector<char> pbfBlob;
	db << "SELECT tile_data FROM tiles WHERE zoom_level=? AND tile_column=? AND tile_row=?" << zoom << col << row >> pbfBlob;
	return pbfBlob;
}

bool MBTiles::readTileAndUncompress(string &data, int zoom, int x, int y, bool isCompressed, bool asGzip) {
	m.lock();
	int tmsY = pow(2,zoom) - 1 - y;
	int exists=0;
	db << "SELECT COUNT(*) FROM tiles WHERE zoom_level=? AND tile_column=? AND tile_row=?" << zoom << x << tmsY >> exists;
	m.unlock();
	if (exists==0) return false;

	m.lock();
	std::vector<char> compressed;
	db << "SELECT tile_data FROM tiles WHERE zoom_level=? AND tile_column=? AND tile_row=?" << zoom << x << tmsY >> compressed;
	m.unlock();
	try {
		bio::stream<bio::array_source> in(compressed.data(), compressed.size());
		bio::filtering_streambuf<bio::input> out;

		if (isCompressed) {
			if (asGzip) { out.push(bio::gzip_decompressor()); }
			else { out.push(bio::zlib_decompressor()); }
		}
		out.push(in);

		std::stringstream decompressed;
		bio::copy(out, decompressed);
		data = decompressed.str();
		return true;
	} catch(std::runtime_error &e) {
		return false;
	}
}
