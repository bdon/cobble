#include "gzip/decompress.hpp"
#include "cbbl/source.hpp"

namespace cbbl {
std::unique_ptr<Source> CreateSource(const std::string &s) {
    std::string ending = ".mbtiles";
    if (s.length() >= ending.length()) {
        if (0 == s.compare (s.length() - ending.length(), ending.length(), ending)) {
            return std::make_unique<MbtilesSource>(s);
        }
    }
    return std::make_unique<HttpSource>(s);
}

HttpSource::HttpSource(const std::string &tile_url) : client(tile_url) {
}

const std::shared_ptr<TileData> HttpSource::fetch(int z, int x, int y) {
    std::ostringstream url_ss;
    url_ss << "/" <<  z << "/" << x << "/" << y << ".pbf";
    std::string url = url_ss.str();
    auto data_response = client.request("GET", url);
    return std::make_shared<TileData>(data_response->content.string(),true,"");
}

HttpSource::~HttpSource() {

}

MbtilesSource::MbtilesSource(const std::string &path) {
    sqlite3_open_v2(path.c_str(), &db,SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX,NULL);
    sqlite3_prepare_v2(db,  "SELECT tile_data FROM tiles WHERE zoom_level = ? AND tile_column = ? AND tile_row = ?", -1, &stmt, 0);
}

const std::shared_ptr<TileData> MbtilesSource::fetch(int z, int x, int y) {
    sqlite3_bind_int(stmt,1,z);
    sqlite3_bind_int(stmt,2,x);
    sqlite3_bind_int(stmt,3,y);
    if (SQLITE_ROW == sqlite3_step(stmt)) {
        const char* res = (char *)sqlite3_column_blob(stmt,0);
        int num_bytes = sqlite3_column_bytes(stmt,0);
        std::string decompressed_data = gzip::decompress(res, num_bytes);
        sqlite3_clear_bindings(stmt);
        sqlite3_reset(stmt);
        return std::make_shared<TileData>(move(decompressed_data),true,"");
    } else {
        sqlite3_clear_bindings(stmt);
        sqlite3_reset(stmt);
        return std::make_shared<TileData>("",false,"row not found");
    }
}

MbtilesSource::~MbtilesSource() {
    sqlite3_finalize(stmt);
    sqlite3_close(db);
}
}