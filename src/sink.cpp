#include "boost/filesystem.hpp"
#include "cbbl/sink.hpp"
#include <sstream>
#include <iostream>

using namespace std;

namespace cbbl {
unique_ptr<Sink> CreateSink(const string &s) {
    string ending = ".mbtiles";
    if (s.length() >= ending.length()) {
        if (0 == s.compare (s.length() - ending.length(), ending.length(), ending)) {
            return make_unique<MbtilesSink>(s);
        }
    }
    return make_unique<FileSink>(s);
}

MbtilesSink::MbtilesSink(const string& s) : mOutput(s) {
    sqlite3_open(mOutput.c_str(), &mDb);
    char * sErrMsg = 0;
    sqlite3_exec(mDb, "BEGIN TRANSACTION", NULL, NULL, &sErrMsg);
    sqlite3_exec(mDb, "CREATE TABLE tiles (zoom_level integer, tile_column integer, tile_row integer, tile_data blob)", NULL, NULL, &sErrMsg);
    sqlite3_prepare_v2(mDb,  "INSERT INTO tiles VALUES (?,?,?,?)", -1, &mStmt, 0);
}

MbtilesSink::~MbtilesSink() {
    char * sErrMsg = 0;
    sqlite3_exec(mDb, "END TRANSACTION", NULL, NULL, &sErrMsg);
    sqlite3_exec(mDb, "CREATE UNIQUE INDEX tile_index on tiles (zoom_level, tile_column, tile_row);", NULL, NULL, &sErrMsg);
    sqlite3_finalize(mStmt);
    sqlite3_close(mDb);
}

void MbtilesSink::writeTile(int res, int z, int x, int y, const string& buf) {
    sqlite3_bind_int(mStmt,1,z);
    sqlite3_bind_int(mStmt,2,x);
    sqlite3_bind_int(mStmt,3,y);
    sqlite3_bind_blob(mStmt,4,buf.data(),buf.size(),SQLITE_STATIC);
    sqlite3_step(mStmt);
    sqlite3_clear_bindings(mStmt);
    sqlite3_reset(mStmt);
}

FileSink::FileSink(const string& s) : mOutput(s) {
    boost::filesystem::create_directory(s);
}

void FileSink::writeTile(int res, int z, int x, int y, const string& buf) {
    string z_dir = mOutput + "/" + to_string(z);
    string zx_dir = mOutput + "/" + to_string(z) + "/" + to_string(x);
    {
        lock_guard<mutex> lock(mCreatedDirsMutex);
        if (!mCreatedDirs.count(z_dir)) {
            boost::filesystem::create_directory(z_dir);
            mCreatedDirs.insert(z_dir);
        }
        if (!mCreatedDirs.count(zx_dir)) {
            boost::filesystem::create_directory(zx_dir);
            mCreatedDirs.insert(zx_dir);
        }
    }
    ofstream outfile;
    stringstream tile_name;
    tile_name << mOutput + "/" + to_string(z) + "/" + to_string(x) + "/" + to_string(y);
    if (res > 1) {
        tile_name << "@" << res << "x";
    }
    tile_name << ".png";
    outfile.open(tile_name.str());
    outfile << buf << endl;
    outfile.close();
}

}