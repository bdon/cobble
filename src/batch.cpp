#include "cxxopts.hpp"

using namespace std;

void cmdBatch(int argc, char * argv[]) {
    cxxopts::Options cmd_options("BATCH", "Batch rasterize tiles");
    cmd_options.add_options()
        ("v,verbose", "Verbose output")
        ("cmd", "Command to run", cxxopts::value<string>())
        ("source", "Source e.g. localhost:8080, example.mbtiles", cxxopts::value<string>())
        ("destination", "Output destination e.g. output.mbtiles", cxxopts::value<string>())
        ("threads", "Number of rendering threads", cxxopts::value<int>())
        ("map", "directory of map style", cxxopts::value<string>())
      ;

    cmd_options.parse_positional({"cmd","source","destination"});
    auto result = cmd_options.parse(argc, argv);
}
