#!/bin/bash
#
# INSTALL DEPENDENCIES
#
# sudo apt install build-essential cmake sqlite3 libsqlite3-dev nlohmann-json3-dev zlib1g-dev wget unzip 7zip
#
#
set -e

## build
if [ ! -e build ]; then
    mkdir build
    cd build
    cmake ..
    make
    cd ..
fi

## download
if [ ! -e data ]; then
    mkdir data
    cd data
    wget https://service.pdok.nl/lv/bag/atom/downloads/lvbag-extract-nl.zip --no-verbose --show-progress --progress=dot:giga 
    cd ..
fi

## extract
if [ ! -e unpack ]; then
    mkdir unpack
    cd unpack
    unzip -o ../data/lvbag-extract-nl.zip
    # unzip the zips containing zips
    find . -name "*.zip" | xargs -P8 -I{} bash -c "unzip {} && rm {}"
    # unzip the zips containing xmls
    find . -name "*.zip" | xargs -P8 -I{} bash -c "unzip {} && rm {}"
    cd ..
fi

## convert
if [ ! -e dist ]; then
    mkdir dist
    cd dist
    ../build/bagconv ../unpack/9999{WPL,OPR,NUM,VBO,LIG,STA,PND}*.xml
    sqlite3 bag.sqlite < ../mkindx
    sqlite3 bag.sqlite < ../make-my-csvs.sql
    7zz a postcodes-nl-geo.7z postcodes-nl-geo.csv 
    7zz a postcodes-nl.7z postcodes-nl.csv 
    cd ..
fi
