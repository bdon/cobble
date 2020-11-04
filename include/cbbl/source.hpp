#pragma once
#include <memory>
#include <string>
#include <fstream>
#include "sqlite3.h"
#define USE_STANDALONE_ASIO true
#include "client_http.hpp"

namespace cbbl {
struct TileData {
    TileData(std::string b, bool o, std::string e) : body(b), ok(o), error(e) {
    }

    std::string body;
    bool ok;
    std::string error;
};

class Source {
   public:
      virtual const std::shared_ptr<TileData> fetch(int z, int x, int y) = 0;
      virtual ~Source() {}
};

std::unique_ptr<Source> CreateSource(const std::string &s);

class FileSource : public Source {

};

class MbtilesSource : public Source {
    public:
        MbtilesSource(const std::string &path);
        ~MbtilesSource(); 
        const std::shared_ptr<TileData> fetch(int z, int x, int y);
    private:
        sqlite3 * db;
        sqlite3_stmt * stmt;

};

class HttpSource : public Source {
    public:
        HttpSource(const std::string &tile_url);
        ~HttpSource();
        const std::shared_ptr<TileData> fetch(int z, int x, int y);

    private:
        SimpleWeb::Client<SimpleWeb::HTTP> client;
};
}