CREATE VIRTUAL TABLE geoindex USING rtree(
   id,              -- Integer primary key
   minX, maxX,      -- Minimum and maximum X coordinate
   minY, maxY,      -- Minimum and maximum Y coordinate
   minLon, maxLon,
   minLat, maxLat,
   +vbo_id TEXT   -- name of the object
);
insert into geoindex(minX,maxX,minY,maxY,minLon,maxLon,minLat,maxLat,vbo_id) select x,x,y,y,lon,lon,lat,lat,id from vbos;
