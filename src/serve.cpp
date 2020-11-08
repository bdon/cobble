#include <fstream>
#include <regex>
#include "cxxopts.hpp"
#define USE_STANDALONE_ASIO true
#include "server_http.hpp"
#include "asio/thread_pool.hpp"
#include "mapnik/image_view.hpp"
#include "mapnik/font_engine_freetype.hpp"

#include "cbbl/tile.hpp"
#include "cbbl/source.hpp"

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

    string map_dir = "debug";
    if (result.count("map")) map_dir = result["map"].as<string>();

    if (map_dir == "debug") {
        mapnik::freetype_engine::register_fonts("/usr/local/lib/mapnik/fonts");
    } else {
        mapnik::freetype_engine::register_fonts(map_dir + "/fonts");
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
                    if(tile_data->ok) {
                        auto img = cbbl::render(map_dir,meta_tile.z,meta_tile.x,meta_tile.y,meta_tile.scale,tile_data->body,data_tile.z,data_tile.x,data_tile.y,metatile_zdiff); // string might be inefficient

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
                        cout << "Error: " << tile_data->error << endl;
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
        string page = R"HTMLLITERAL(
<!DOCTYPE html>
<html>
    <head>
        <meta charset="utf-8" />
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <link rel="stylesheet" href="https://unpkg.com/leaflet@1.6.0/dist/leaflet.css"
   integrity="sha512-xwE/Az9zrjBIphAcBb3F6JVqxf46+CDLwfLMHloNu6KEQCAWi6HcDUbeOfBIptF7tcCzusKFjFw2yuvEpDL9wQ=="
   crossorigin=""/>
        <script src="https://unpkg.com/leaflet@1.6.0/dist/leaflet.js"
           integrity="sha512-gZwIG9x3wUXg2hdXF6+rVkLF/0Vi9U8D2Ntg4Ga5I5BZpVkVxlJWbSQtXPSiUTtC0TjtGOmxa1AJPuV0CPthew=="
           crossorigin=""></script>
        <script src="https://cdn.protomaps.com/leaflet-hash/leaflet-hash.js"></script>
        <style>
            body, #map {
                height:100vh;
                margin:0px;
            }
        </style>
    </head>
    <body>
        <div id="map"></div>
        <script>
            var ratio = '';
            if (window.devicePixelRatio > 2) ratio = '@3x';
            else if (window.devicePixelRatio == 2) ratio = '@2x';
            var map = L.map('map').setView([$CENTER_Y,$CENTER_X],$CENTER_ZOOM);
            var hash = new L.Hash(map)
            L.tileLayer('/{z}/{x}/{y}{r}.png', {
                attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors',
                r: ratio, 
                maxZoom: 21,
                bounds: [[$MIN_Y,$MIN_X],[$MAX_Y,$MAX_X]]
            }).addTo(map);
        </script>
    </body>
</html>
)HTMLLITERAL";
        page = regex_replace(page, regex("\\$CENTER_X"), get<0>(center));
        page = regex_replace(page, regex("\\$CENTER_Y"), get<1>(center));
        page = regex_replace(page, regex("\\$CENTER_ZOOM"), get<2>(center));
        page = regex_replace(page, regex("\\$MIN_X"), get<0>(bounds));
        page = regex_replace(page, regex("\\$MIN_Y"), get<1>(bounds));
        page = regex_replace(page, regex("\\$MAX_X"), get<2>(bounds));
        page = regex_replace(page, regex("\\$MAX_Y"), get<3>(bounds));
        response->write(page);
    };

    server.start();
    pool.join();
}