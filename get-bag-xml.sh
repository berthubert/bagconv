#!/bin/bash
wget https://service.pdok.nl/lv/bag/atom/downloads/lvbag-extract-nl.zip
rm -rf bag
mkdir bag
cd bag
unzip ../lvbag-extract-nl.zip
echo 9999{WPL,OPR,NUM,VBO,LIG,STA,PND}*.zip | xargs -n1 -P8 unzip

