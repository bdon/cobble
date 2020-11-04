#include <iostream>
#include <fstream>
#include "mapnik/map.hpp"
#include "mapnik/agg_renderer.hpp"
#include "mapnik/image_util.hpp"
#include "mapnik/load_map.hpp"
#include "vector_tile_datasource_pbf.hpp"
#include "vector_tile_projection.hpp"
#include "vector_tile_tile.hpp"
#include "vtzero/vector_tile.hpp"

namespace cbbl {
mapnik::image_rgba8 render(int z, int x, int y, int tile_scale, const protozero::data_view &data, int dz, int dx, int dy, int metatile_zdiff) {
    int dim = 256 * tile_scale * (1 << metatile_zdiff);
    mapnik::Map map(dim,dim,mapnik::MAPNIK_GMERC_PROJ);
    map.set_buffer_size(64 * tile_scale);
    vtzero::vector_tile tile{data};
    std::map<std::string,std::shared_ptr<mapnik::vector_tile_impl::tile_datasource_pbf>> datasources;

    while (auto layer = tile.next_layer()) {
        std::string name{layer.name()};
        protozero::pbf_reader layer_reader(layer.data());
        datasources[name] = std::make_shared<mapnik::vector_tile_impl::tile_datasource_pbf>(layer_reader,dx,dy,dz,false);
    }

    std::ifstream in("assets/styles/style.layers");
    std::string line;
    while (std::getline(in,line)) {
        std::istringstream iss(line);
        std::vector<std::string> results(std::istream_iterator<std::string>{iss},std::istream_iterator<std::string>());
        if (results.size() < 2) continue;
        if (datasources.count(results[0])) {
            mapnik::layer lyr(results[1],map.srs());
            lyr.set_datasource(datasources.at(results[0]));
            lyr.add_style(results[1]);
            map.add_layer(lyr);
        }
    }

    mapnik::load_map(map,"assets/styles/style.xml");
    auto bbox = mapnik::vector_tile_impl::tile_mercator_bbox(x,y,z);
    map.zoom_to_box(bbox);

    mapnik::image_rgba8 buf(map.width(),map.height());
    mapnik::agg_renderer<mapnik::image_rgba8> ren(map,buf,tile_scale);
    ren.apply();
    return buf;
}
}