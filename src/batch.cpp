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
      ;

    cmd_options.parse_positional({"cmd","source","destination"});
    auto result = cmd_options.parse(argc, argv);

    // set up output
    auto output = result["destination"].as<string>();
    if (boost::filesystem::exists(output) && !result.count("overwrite")) {
        cout << "Target output " << output << " exists." << endl;
        exit(1);
    }
    if (boost::filesystem::exists(output)) {
        boost::filesystem::remove_all(output);
    }

    string map_dir = "debug";
    if (result.count("map")) map_dir = result["map"].as<string>();
    if (map_dir == "debug") {
        mapnik::freetype_engine::register_fonts("/usr/local/lib/mapnik/fonts");
    } else {
        mapnik::freetype_engine::register_fonts(map_dir + "/fonts");
    }

    auto source = cbbl::MbtilesSource(result["source"].as<string>());
    auto iter = cbbl::MbtilesSource::Iterator(source);
    boost::filesystem::create_directory(output);

    int threads = 4;
    if (result.count("threads")) threads = result["threads"].as<int>();
    asio::thread_pool pool(threads);

    chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

    size_t scale = 2;
    int tiles_rendered = 0;
    set<string> created_dirs;
    mutex created_dirs_mutex;
    while (iter.next()) {
        int data_z = iter.z;
        int data_x = iter.x;
        int data_y = iter.y;
        int meta_z = data_z;
        int meta_x = data_x;
        int meta_y = data_y;
        string data = iter.data;

        asio::post(pool, [&output,&map_dir,scale,data_z,data_x,data_y,data,meta_z,meta_x,meta_y,&created_dirs,&created_dirs_mutex] {
            auto img = cbbl::render(map_dir,meta_z,meta_x,meta_y,scale,data,data_z,data_x,data_y,2);
            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    mapnik::image_view_rgba8 cropped{256*scale*i,256*scale*j,256*scale,256*scale,img};
                    auto buf = mapnik::save_to_string(cropped,"png");
                    int display_z = data_z + 2;
                    int display_x = data_x * 4 + i;
                    int display_y = data_y * 4 + j;
                    string z_dir = output + "/" + to_string(display_z);
                    string zx_dir = output + "/" + to_string(display_z) + "/" + to_string(display_x);
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
                    outfile.open(output + "/" + to_string(display_z) + "/" + to_string(display_x) + "/" + to_string(display_y) + "@2x.png");
                    outfile << buf << endl;
                    outfile.close();
                }
            }
        });
        tiles_rendered += 16;
    }

    pool.join();
    auto bounds = source.bounds();
    auto center = source.center();
    ofstream index;
    index.open(output + "/index.html");
    index << cbbl::viewer(center,bounds) << endl;
    index.close();

    chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    cout << "Rendered " << tiles_rendered << " tiles in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() / 1000.0 << " seconds." << endl;
}
