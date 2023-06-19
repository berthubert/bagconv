#!/bin/sh

rm -f pcodes.csv
sqlite3 bag.sqlite < make-csvs.sql
rm pcodes-geo-$(python3 get-date-of-xml.py).zip
zip pcodes-geo-$(python3 get-date-of-xml.py).zip pcodes-geo.csv 
rm pcodes-$(python3 get-date-of-xml.py).zip
zip pcodes-$(python3 get-date-of-xml.py).zip pcodes.csv 
