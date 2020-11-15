#!/bin/bash
set -e
FILENAME=dist/cobble-$1-$2.tgz
tar -cvzf $FILENAME cbbl debug libmapnik.so.4.0 libmapnik.so libboost_filesystem.so libboost_filesystem.so.1.74.0 libboost_regex.so libboost_regex.so.1.74.0 
echo "created $FILENAME"
