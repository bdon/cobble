#pragma once
#include <memory>
#include <string>
#include <fstream>
#include <optional>
#include "sqlite3.h"
#define USE_STANDALONE_ASIO true
#include "client_http.hpp"

namespace cbbl {
class Source {
   public:
      virtual const std::optional<std::string> fetch(int z, int x, int y) = 0;
      virtual ~Source() {}
      virtual const std::tuple<std::string,std::string,std::string> center() { return {"0","0","0"}; };
      virtual const std::tuple<std::string,std::string,std::string,std::string> bounds() { return {"-180","-90","180","90"}; };
};

std::unique_ptr<Source> CreateSource(const std::string &s, bool gzip);

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

        MbtilesSource(const std::string &path, bool gzip);
        ~MbtilesSource(); 
        const std::optional<std::string> fetch(int z, int x, int y) override;
        const std::tuple<std::string,std::string,std::string> center() override;
        const std::tuple<std::string,std::string,std::string,std::string> bounds() override;

        const std::vector<std::pair<int,int>> zoom_count();

    private:
        sqlite3 * db;
        sqlite3_stmt * stmt;
        bool gzip_;
};

class HttpSource : public Source {
    public:
        HttpSource(const std::string &tile_url);
        ~HttpSource();
        const std::optional<std::string> fetch(int z, int x, int y) override;

    private:
        SimpleWeb::Client<SimpleWeb::HTTP> client;
};
}