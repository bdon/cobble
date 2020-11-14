#include "cxxopts.hpp"
#include "cbbl/source.hpp"

using namespace std;

void cmdBatch(int argc, char * argv[]) {
    cxxopts::Options cmd_options("BATCH", "Batch rasterize tiles");
    cmd_options.add_options()
        ("v,verbose", "Verbose output")
        ("cmd", "Command to run", cxxopts::value<string>())
        ("source", "Source e.g. example.mbtiles", cxxopts::value<string>())
        ("destination", "Output destination e.g. output.mbtiles", cxxopts::value<string>())
        ("threads", "Number of rendering threads", cxxopts::value<int>())
        ("map", "directory of map style", cxxopts::value<string>())
      ;

    cmd_options.parse_positional({"cmd","source","destination"});
    auto result = cmd_options.parse(argc, argv);

    auto source = cbbl::MbtilesSource(result["source"].as<string>());

    auto iter = cbbl::MbtilesSource::Iterator(source);

    while (iter.next()) {
        cout << iter.z << " " << iter.x << " " << iter.y << endl;
    }
}
