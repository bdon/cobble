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
      virtual const std::map<std::string,std::string> metadata() { return {}; }
      virtual const std::tuple<std::string,std::string,std::string> center() { return {"0","0","0"}; };
      virtual const std::tuple<std::string,std::string,std::string,std::string> bounds() { return {"-180","-90","180","90"}; };
};

std::unique_ptr<Source> CreateSource(const std::string &s);

class FileSource : public Source {

};

class MbtilesSource : public Source {
    public:
        class Iterator {
            public:
            Iterator(MbtilesSource &src);
            ~Iterator();
            bool next();

            int z;
            int x;
            int y;
            std::string data; // already decompressed

            private:
            sqlite3_stmt *stmt;
        };

        MbtilesSource(const std::string &path);
        ~MbtilesSource(); 
        const std::shared_ptr<TileData> fetch(int z, int x, int y) override;
        const std::tuple<std::string,std::string,std::string> center() override;
        const std::tuple<std::string,std::string,std::string,std::string> bounds() override;
        const std::map<std::string,std::string> metadata() override;

        const std::vector<std::pair<int,int>> zoom_count();

    private:
        sqlite3 * db;
        sqlite3_stmt * stmt;

};

class HttpSource : public Source {
    public:
        HttpSource(const std::string &tile_url);
        ~HttpSource();
        const std::shared_ptr<TileData> fetch(int z, int x, int y) override;

    private:
        SimpleWeb::Client<SimpleWeb::HTTP> client;
};
}