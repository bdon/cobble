#pragma once
#include <regex>

namespace cbbl {
    static std::string viewer(const std::tuple<std::string,std::string,std::string> &center, const std::tuple<std::string,std::string,std::string,std::string> &bounds) {
        std::string page = R"HTMLLITERAL(
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
        page = std::regex_replace(page, std::regex("\\$CENTER_X"), std::get<0>(center));
        page = std::regex_replace(page, std::regex("\\$CENTER_Y"), std::get<1>(center));
        page = std::regex_replace(page, std::regex("\\$CENTER_ZOOM"), std::get<2>(center));
        page = std::regex_replace(page, std::regex("\\$MIN_X"), std::get<0>(bounds));
        page = std::regex_replace(page, std::regex("\\$MIN_Y"), std::get<1>(bounds));
        page = std::regex_replace(page, std::regex("\\$MAX_X"), std::get<2>(bounds));
        page = std::regex_replace(page, std::regex("\\$MAX_Y"), std::get<3>(bounds));
        return page;
    }
}
