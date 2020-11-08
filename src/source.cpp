#include "gzip/decompress.hpp"
#include "cbbl/source.hpp"

using namespace std;

namespace cbbl {
unique_ptr<Source> CreateSource(const string &s) {
    string ending = ".mbtiles";
    if (s.length() >= ending.length()) {
        if (0 == s.compare (s.length() - ending.length(), ending.length(), ending)) {
            return make_unique<MbtilesSource>(s);
        }
    }
    return make_unique<HttpSource>(s);
}

HttpSource::HttpSource(const string &tile_url) : client(tile_url) {
}

const shared_ptr<TileData> HttpSource::fetch(int z, int x, int y) {
    ostringstream url_ss;
    url_ss << "/" <<  z << "/" << x << "/" << y << ".pbf";
    string url = url_ss.str();
    auto data_response = client.request("GET", url);
    return make_shared<TileData>(data_response->content.string(),true,"");
}

HttpSource::~HttpSource() {

}

MbtilesSource::MbtilesSource(const string &path) {
    sqlite3_open_v2(path.c_str(), &db,SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX,NULL);
    sqlite3_prepare_v2(db,  "SELECT tile_data FROM tiles WHERE zoom_level = ? AND tile_column = ? AND tile_row = ?", -1, &stmt, 0);
}

const tuple<string,string,string> MbtilesSource::center() {
    sqlite3_stmt * metadata_stmt;
    sqlite3_prepare_v2(db,  "SELECT value FROM metadata WHERE name = \"center\"", -1, &metadata_stmt, 0);
    if (SQLITE_ROW == sqlite3_step(metadata_stmt)) {
        const char* res = (char *)sqlite3_column_text(metadata_stmt,0);
        stringstream s_stream(res);
        string x;
        getline(s_stream, x, ',');
        string y;
        getline(s_stream, y, ',');
        string zoom;
        getline(s_stream, zoom, ',');
        return {x,y,zoom};
    } else {
        cout << "Malformed mbtiles" << endl;
    }
    return {"0","0","0"};
}

const tuple<string,string,string,string> MbtilesSource::bounds() {
    sqlite3_stmt * metadata_stmt;
    sqlite3_prepare_v2(db,  "SELECT value FROM metadata WHERE name = \"bounds\"", -1, &metadata_stmt, 0);
    if (SQLITE_ROW == sqlite3_step(metadata_stmt)) {
        const char* res = (char *)sqlite3_column_text(metadata_stmt,0);
        stringstream s_stream(res);
        string min_x;
        getline(s_stream, min_x, ',');
        string min_y;
        getline(s_stream, min_y, ',');
        string max_x;
        getline(s_stream, max_x, ',');
        string max_y;
        getline(s_stream, max_y, ',');
        return {min_x,min_y,max_x,max_y};
    } else {
        cout << "Malformed mbtiles" << endl;
    }
    return {"-180","-90","180","90"};
}

const shared_ptr<TileData> MbtilesSource::fetch(int z, int x, int y) {
    sqlite3_bind_int(stmt,1,z);
    sqlite3_bind_int(stmt,2,x);
    sqlite3_bind_int(stmt,3,y);
    if (SQLITE_ROW == sqlite3_step(stmt)) {
        const char* res = (char *)sqlite3_column_blob(stmt,0);
        int num_bytes = sqlite3_column_bytes(stmt,0);
        string decompressed_data = gzip::decompress(res, num_bytes);
        sqlite3_clear_bindings(stmt);
        sqlite3_reset(stmt);
        return make_shared<TileData>(move(decompressed_data),true,"");
    } else {
        sqlite3_clear_bindings(stmt);
        sqlite3_reset(stmt);
        return make_shared<TileData>("",false,"row not found");
    }
}

MbtilesSource::~MbtilesSource() {
    sqlite3_finalize(stmt);
    sqlite3_close(db);
}
}