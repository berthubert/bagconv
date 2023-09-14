#!/bin/bash
./bagconv bag/9999{WPL,OPR,NUM,VBO,LIG,STA,PND}*.xml
sqlite3 bag.sqlite  < mkindx 
sqlite3 bag.sqlite  < geo-queries 
