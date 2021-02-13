#include "boost/filesystem.hpp"
#include "cbbl/sink.hpp"
#include <sstream>

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

MbtilesSink::MbtilesSink(const string& s) {

}

void MbtilesSink::writeTile(int res, int z, int x, int y, const string& buf) {

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