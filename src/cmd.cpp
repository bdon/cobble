#include <iostream>
#include <fstream>
#include <vector>
#include "mapnik/image_util.hpp"
#include "cbbl/cmd.hpp"
#include "cbbl/tile.hpp"

using namespace std;

void printHelp() {
    cout << "Command not recognized." << endl;
    cout << "Commands: tile | batch | serve" << endl;
    exit(1);
}

int main(int argc, char **argv)
{
    vector<string> args(argv, argv+argc);
    if (args.size() < 2) {
        printHelp();
    }
    if (args[1] == "tile") {
        ifstream stream(args[2],std::ios_base::in|std::ios_base::binary);
        std::string buffer(std::istreambuf_iterator<char>(stream.rdbuf()),(std::istreambuf_iterator<char>()));
        auto img = cbbl::render("debug",0,0,0,2,buffer,nullopt,0,0,0,2);
        mapnik::save_to_file(img ,args[3],"png");
    } else if (args[1] == "batch") {
        cmdBatch(argc,argv);
    } else if (args[1] == "serve") {
        cmdServe(argc,argv);
    } else {
        printHelp();
    }
}

