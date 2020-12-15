#include <set>
#include "cxxopts.hpp"
#include "boost/filesystem.hpp"
#include "mapnik/font_engine_freetype.hpp"
#include "mapnik/image_view.hpp"
#include "asio/thread_pool.hpp"
#include "cbbl/source.hpp"
#include "cbbl/tile.hpp"
#include "cbbl/viewer.hpp"

using namespace std;

void cmdBatch(int argc, char * argv[]) {
    cxxopts::Options cmd_options("BATCH", "Batch rasterize tiles");
    cmd_options.add_options()
        ("v,verbose", "Verbose output")
        ("cmd", "Command to run", cxxopts::value<string>())
        ("source", "Source e.g. example.mbtiles", cxxopts::value<string>())
        ("destination", "Output dir or destination e.g. output, output.mbtiles", cxxopts::value<string>())
        ("overwrite", "Overwrite output", cxxopts::value<bool>())
        ("threads", "Number of rendering threads", cxxopts::value<int>())
        ("map", "directory of map style", cxxopts::value<string>())
        ("maxzoom", "maximum display zoom level (default 16, maximum 21", cxxopts::value<int>())
        ("resolutions", "comma-separated resolutions: default 1,2", cxxopts::value<vector<int>>())
      ;

    cmd_options.parse_positional({"cmd","source","destination"});
    auto result = cmd_options.parse(argc, argv);

    int maxzoom = 16;
    if (result.count("maxzoom")) {
        maxzoom = result["maxzoom"].as<int>();
        if (maxzoom > 21) {
            cout << "max supported zoom is 21." << endl;
            exit(1);
        }
    }

    vector<int> resolutions = {1,2};
    if (result.count("resolutions")) {
        resolutions = result["resolutions"].as<vector<int>>();
    }

    cout << "rendering zooms 0-" << maxzoom << " at resolutions";
    for (auto r : resolutions) cout << " @" << r << "x";
    cout << endl;

    auto source = cbbl::MbtilesSource(result["source"].as<string>());

    int total_tiles = 0;
    int num_resolutions = resolutions.size();

    for (auto p : source.zoom_count()) {
        int zoom_level = get<0>(p);
        int count = get<1>(p);
        if (zoom_level == 0) {
            total_tiles += (16 + 4 + 1) * num_resolutions;
        } else if (zoom_level < 14) {
            if (zoom_level <= maxzoom - 2) {
                total_tiles += count * 16 * num_resolutions;
            }
        } else {
            // zoom level == 14
            for (int z = 16; z <= maxzoom; z++) {
                total_tiles += count * (2 << (z - 16)) * 16 * num_resolutions;
            }
        }
    }

    cout << "Total tiles: " << total_tiles << " Continue? (N) :";
    char ans = 'N';
    cin >> ans;

    // set up output
    auto output = result["destination"].as<string>();
    if (boost::filesystem::exists(output) && !result.count("overwrite")) {
        cout << "Target output " << output << " exists." << endl;
        exit(1);
    }
    if (boost::filesystem::exists(output)) {
        boost::filesystem::remove_all(output);
    }

    string map_dir = "example";
    if (result.count("map")) map_dir = result["map"].as<string>();

    if (boost::filesystem::exists(map_dir + "/fonts")) {
        mapnik::freetype_engine::register_fonts(map_dir + "/fonts");
    } else {
        mapnik::freetype_engine::register_fonts("/usr/local/lib/mapnik/fonts");
    }

    auto iter = cbbl::MbtilesSource::Iterator(source);
    boost::filesystem::create_directory(output);

    int threads = 4;
    if (result.count("threads")) threads = result["threads"].as<int>();
    asio::thread_pool pool(threads);

    chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

    set<string> created_dirs;
    mutex created_dirs_mutex;

    auto writeTile = [&output, &created_dirs, &created_dirs_mutex](int res, int z, int x, int y, const string& buf) {
        string z_dir = output + "/" + to_string(z);
        string zx_dir = output + "/" + to_string(z) + "/" + to_string(x);
        {
            lock_guard<mutex> lock(created_dirs_mutex);
            if (!created_dirs.count(z_dir)) {
                boost::filesystem::create_directory(z_dir);
                created_dirs.insert(z_dir);
            }
            if (!created_dirs.count(zx_dir)) {
                boost::filesystem::create_directory(zx_dir);
                created_dirs.insert(zx_dir);
            }
        }
        ofstream outfile;
        stringstream tile_name;
        tile_name << output + "/" + to_string(z) + "/" + to_string(x) + "/" + to_string(y);
        if (res > 1) {
            tile_name << "@" << res << "x";
        }
        tile_name << ".png";
        outfile.open(tile_name.str());
        outfile << buf << endl;
        outfile.close();
    };

    while (iter.next()) {
        int data_z = iter.z;
        if (data_z > maxzoom - 2) continue;
        int data_x = iter.x;
        int data_y = iter.y;
        string data = iter.data;

        asio::post(pool, [&writeTile,&resolutions,&output,&map_dir,data_z,data_x,data_y,data,&created_dirs,&created_dirs_mutex,maxzoom] {
            for (size_t res : resolutions) { // 1, 2 or 3
                // metatile = datatile
                auto img = cbbl::render(map_dir,data_z,data_x,data_y,res,data,data_z,data_x,data_y,2);
                for (int i = 0; i < 4; i++) {
                    for (int j = 0; j < 4; j++) {
                        mapnik::image_view_rgba8 cropped{256*res*i,256*res*j,256*res,256*res,img};
                        auto buf = mapnik::save_to_string(cropped,"png");
                        int display_z = data_z + 2;
                        int display_x = data_x * 4 + i;
                        int display_y = data_y * 4 + j;
                        writeTile(res,display_z,display_x,display_y,buf);
                    }
                }

                // special case data tile 0,0,0 to output display levels 0 and 1
                if (data_z == 0) {
                    {
                        auto img = cbbl::render(map_dir,0,0,0,res,data,0,0,0,1);
                        for (int i = 0; i < 2; i++) {
                            for (int j = 0; j < 2; j++) {
                                mapnik::image_view_rgba8 cropped{256*res*i,256*res*j,256*res,256*res,img};
                                auto buf = mapnik::save_to_string(cropped,"png");
                                writeTile(res,1,i,j,buf);
                            }
                        }
                    }

                    {
                        auto img = cbbl::render(map_dir,0,0,0,res,data,0,0,0,0);
                        auto buf = mapnik::save_to_string(img,"png");
                        writeTile(res,0,0,0,buf);
                    }
                }

                // TODO special case data tile 14: it outputs 17 to maxzoom
                if (data_z == 14) {
                    for (int meta_z = 15; meta_z <= maxzoom - 2; meta_z++) {
                        int diff = meta_z - 14;
                        for (int u = 0; u < 1 << diff; u++) {
                            for (int v = 0; v < 1 << diff; v++) {
                                int meta_x = data_x * (1 << diff) + u;
                                int meta_y = data_y * (1 << diff) + v;
                                auto img = cbbl::render(map_dir,meta_z,meta_x,meta_y,res,data,14,data_x,data_y,2);
                                for (int i = 0; i < 4; i++) {
                                    for (int j = 0; j < 4; j++) {
                                        mapnik::image_view_rgba8 cropped{256*res*i,256*res*j,256*res,256*res,img};
                                        auto buf = mapnik::save_to_string(cropped,"png");
                                        int display_z = meta_z + 2;
                                        int display_x = meta_x * 4 + i;
                                        int display_y = meta_y * 4 + j;
                                        writeTile(res,display_z,display_x,display_y,buf);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        });
    }

    pool.join();
    auto bounds = source.bounds();
    auto center = source.center();
    ofstream index;
    index.open(output + "/index.html");
    index << cbbl::viewer(center,bounds) << endl;
    index.close();

    chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    cout << "Finished in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() / 1000.0 << " seconds." << endl;
}
