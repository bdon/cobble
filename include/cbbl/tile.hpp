#pragma once
#include <string>
#include <optional>
#include "mapnik/image_util.hpp"
#include "protozero/data_view.hpp"

namespace cbbl {
	// z, x, y: the "metatile" coordinates, where a metatile is a single image corresponding to multiple display tiles;
	// label placement happens per metatile.
	// dz, dx, dy: the "datatile" coordinates, which correspond to the data in buffer
    mapnik::image_rgba8 render(const std::string &map_dir, int z, int x, int y, int tile_scale, const std::optional<std::string> &v_data, const std::optional<std::string> &t_data, int dz, int dx, int dy, int metatile_zdiff);
}
