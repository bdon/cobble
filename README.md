# cobble
Render vector tiles to raster images with [Mapnik](https://github.com/mapnik/mapnik).

Cobble serves raster tiles through an embedded HTTP server; with a given set of vector tiles: 

    cbbl serve dataset.mbtiles
    
There is a very basic Mapnik style in the `debug` directory. You can download packages of vector tiles based on fresh OpenStreetMap data at [Protomaps Map Bundles](http://protomaps.com/bundles). Make sure to choose the "mbtiles" output format.

## Key features

* Meta-tiles: tiles are rendered in batches; by default one vector tile is rendered as 4x4 raster tiles. This is necessary for label placement across tiles.
* Pixel density: tiles can rendered at 72 dpi, @2x and @3x resolutions.

## Use

Linux: libcairo-dev libtiff-dev libharfbuzz-dev

## Build

* Mapnik is expected to be an a sibling directory
* Configure your mapnik build with `./configure FULL_LIB_PATH=False INPUT_PLUGINS='geojson,raster,shape,topojson' ENABLE_LOG=True`

## Alternatives
* [mod_tile](https://github.com/openstreetmap/mod_tile)
* [tirex](https://github.com/openstreetmap/tirex)
* [TileStache](http://tilestache.org)
* [kosmtik](https://github.com/kosmtik/kosmtik)
* [TileMill](https://github.com/tilemill-project/tilemill)
* [Kartotherian](https://github.com/kartotherian/kartotherian)

## Other Stuff
* [The smallest 256x256 single-color PNG file](https://www.mjt.me.uk/posts/smallest-png/)
