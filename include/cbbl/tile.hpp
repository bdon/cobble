#pragma once
#include "mapnik/image_util.hpp"
#include "protozero/data_view.hpp"
#include <string>

namespace cbbl {
    mapnik::image_rgba8 render(int z, int x, int y, int tile_scale, const protozero::data_view &buffer, int dz, int dx, int dy, int metatile_zdiff);
}
