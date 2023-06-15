#pragma once

struct WGS84Pos
{
  double lat;
  double lon;
};

WGS84Pos rd2wgs84 (double x, double y);

