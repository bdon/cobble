#include <fstream>
#include "cxxopts.hpp"
#define USE_STANDALONE_ASIO true
#include "server_http.hpp"
#include "asio/thread_pool.hpp"
#include "boost/filesystem.hpp"
#include "mapnik/image_view.hpp"
#include "mapnik/font_engine_freetype.hpp"
#include "mapnik/debug.hpp"

#include "cbbl/tile.hpp"
#include "cbbl/source.hpp"
#include "cbbl/viewer.hpp"

using namespace std;
using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;

thread_local unique_ptr<cbbl::Source> tSource;

class Tile {
public:
    int z;
    int x;
    int y;
    int scale;
    int display_level;
};

std::ostream & operator<<(std::ostream & stream, const Tile & t) {
    stream << t.z << "/" << t.x << "/" << t.y << "@" << t.scale << "x";
    return stream;
}

struct TileCompare {
    bool operator() (const Tile& lhs, const Tile &rhs) const {
        if (lhs.display_level == rhs.display_level) {
            if (lhs.scale == rhs.scale) {
                if (lhs.z == rhs.z) {
                    if (lhs.y == rhs.y) {
                        return lhs.x < rhs.x;
                    } else return lhs.y < rhs.y;
                } else return lhs.z < rhs.z;
            } else return lhs.scale < rhs.scale;
        } else return lhs.display_level < rhs.display_level;
    }
};

static int METATILE_LEVEL = 2;

mutex gMutex;
map<Tile,vector<pair<Tile,shared_ptr<HttpServer::Response>>>,TileCompare> mState;

void cmdServe(int argc, char * argv[]) {
    cxxopts::Options cmd_options("SERVE", "Serve raster tiles");
    cmd_options.add_options()
        ("v,verbose", "Verbose output")
        ("cmd", "Command to run", cxxopts::value<string>())
        ("source", "Source e.g. localhost:8080, example.mbtiles", cxxopts::value<string>())
        ("cors", "Allow all CORS origins")
        ("port", "HTTP port", cxxopts::value<int>())
        ("threads", "Number of rendering threads", cxxopts::value<int>())
        ("map", "directory of map style", cxxopts::value<string>())
      ;

    cmd_options.parse_positional({"cmd","source"});
    auto result = cmd_options.parse(argc, argv);

    int threads = 4;
    if (result.count("threads")) threads = result["threads"].as<int>();
    asio::thread_pool pool(threads);

    HttpServer server;
    int port = 8090;
    if (result.count("port")) port = result["port"].as<int>();
    server.config.port = port;

    bool cors = result.count("cors");
    auto source_str = result["source"].as<string>();

    auto source = cbbl::CreateSource(source_str);
    auto bounds = source->bounds();
    auto center = source->center();

    mapnik::logger::instance().set_severity(mapnik::logger::none);

    string map_dir = "example";
    if (result.count("map")) map_dir = result["map"].as<string>();

    if (boost::filesystem::exists(map_dir + "/fonts")) {
        mapnik::freetype_engine::register_fonts(map_dir + "/fonts");
    } else {
        mapnik::freetype_engine::register_fonts("/usr/local/lib/mapnik/fonts");
    }

    cout << "source: " << source_str << " with " << threads << " threads on port " << port << endl;

    server.resource["^/([0-9]+)/([0-9]+)/([0-9]+)(@([2-3])x)?.png$"]["GET"] = [cors,&pool,source_str,map_dir](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
        // displaytile, metatile and datatile
        // the URL params are the Display tiles
        int32_t display_z = stoi(request->path_match[1]);
        int32_t display_x = stoi(request->path_match[2]);
        int32_t display_y = stoi(request->path_match[3]);
        int display_scale = 1;
        if (request->path_match[5].length() > 0) {
            display_scale = stoi(request->path_match[5]);
        }
        Tile display_tile{display_z,display_x,display_y,display_scale,display_z};

        Tile meta_tile;
        int metatile_zdiff = 2;
        if (display_z < METATILE_LEVEL) {
            metatile_zdiff = display_z;
            meta_tile = Tile{0,0,0,display_scale,display_z};
        } else {
            meta_tile = Tile{display_z-METATILE_LEVEL,display_x/(1 << METATILE_LEVEL),display_y/(1 << METATILE_LEVEL),display_scale,display_z};
        }

        {
            lock_guard<mutex> lock(gMutex);
            if (mState.count(meta_tile)) {
                mState[meta_tile].push_back(make_pair(display_tile,response));
            } else {
                vector<pair<Tile,shared_ptr<HttpServer::Response>>> v;
                v.emplace_back(display_tile,response);
                mState.emplace(meta_tile,move(v));
                asio::post(pool, [meta_tile,source_str,map_dir,cors,metatile_zdiff] {
                    if (!tSource) tSource = cbbl::CreateSource(source_str);
                    chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
                    // calculate the datatile for this metatile
                    int data_z = meta_tile.z;
                    int data_x = meta_tile.x;
                    int data_y = meta_tile.y;
                    if (meta_tile.z > 14) {
                        data_z = 14;
                        data_x = data_x / (1 << (meta_tile.z-14));
                        data_y = data_y / (1 << (meta_tile.z-14));
                    }
                    Tile data_tile{data_z,data_x,data_y,meta_tile.scale};
                    auto tile_data = tSource->fetch(data_z,data_x,data_y);
                    if(tile_data) {
                        auto img = cbbl::render(map_dir,meta_tile.z,meta_tile.x,meta_tile.y,meta_tile.scale,tile_data,data_tile.z,data_tile.x,data_tile.y,metatile_zdiff); // string might be inefficient

                        chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
                        cout << meta_tile.z << "/" << meta_tile.x << "/" << meta_tile.y <<  "@" << meta_tile.scale << ":" << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << " ms" << endl;

                        vector<pair<Tile,shared_ptr<HttpServer::Response>>> responses; 
                        {
                            lock_guard<mutex> lock(gMutex);
                            responses = mState.at(meta_tile);
                            mState.erase(meta_tile);
                        }

                        // write display tile responses
                        for (auto &resp : responses) {
                            auto display_tile = get<0>(resp);
                            size_t scale = display_tile.scale;

                            size_t offset_x = (display_tile.x - meta_tile.x * (1 << metatile_zdiff));
                            size_t offset_y = (display_tile.y - meta_tile.y * (1 << metatile_zdiff));
                            mapnik::image_view_rgba8 cropped{256*scale*offset_x,256*scale*offset_y,256*scale,256*scale,img};
                            auto buf = mapnik::save_to_string(cropped,"png");
                            SimpleWeb::CaseInsensitiveMultimap headers;
                            if (cors) headers.emplace("Access-Control-Allow-Origin","*");
                            headers.emplace("Content-Type","image/png");
                            get<1>(resp)->write(buf,headers);
                        }
                    } else {
                        // the tile will not render, meaning we need to evict the entire metatile and resolve all promises
                        cout << "Error: tile not found in any sources" << endl;
                        vector<pair<Tile,shared_ptr<HttpServer::Response>>> responses; 
                        {
                            lock_guard<mutex> lock(gMutex);
                            responses = mState.at(meta_tile);
                            mState.erase(meta_tile);
                        }
                        for (auto &resp : responses) {
                            get<1>(resp)->write("Error");
                        }
                    }
                });
            }
        }
    };


    server.resource["^/queue$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
        ostringstream ss;
        {
            lock_guard<mutex> lock(gMutex);
            for (auto const &pair : mState) {
                ss << pair.first;
                for (auto const &r : pair.second) {
                    ss << " " << r.first;
                }
                ss << endl;
            }
        }
        response->write(ss.str());
    };

    server.resource["^/$"]["GET"] = [center,bounds](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
        string page = cbbl::viewer(center,bounds);
        response->write(page);
    };

    server.start();
    pool.join();
}