#include "rd2wgs84.hh"
#include <math.h>

// lifted from https://github.com/glenndehaan/rd-to-wgs84 by Glenn de Haan. Don't know where he got it from.
// maybe https://web.archive.org/web/20061207170256/http://www.dekoepel.nl/pdf/Transformatieformules.pdf
// also cool (in Dutch) https://io.osgeo.nl/sitecontent/events/RDMiniSeminar2016/LennardHuisman.pdf
// also good to read: https://github.com/digitaldutch/BAG_parser/blob/master/docs/Benaderingsformules_RD_WGS.pdf
WGS84Pos rd2wgs84 (double x, double y)
{
    constexpr double x0 = 155000.000;
    constexpr double y0 = 463000.000;

    constexpr double f0 = 52.156160556;
    constexpr double l0 = 5.387638889;

    constexpr double a01 = 3236.0331637;
    constexpr double b10 = 5261.3028966;
    constexpr double a20 = -32.5915821;
    constexpr double b11 = 105.9780241;
    constexpr double a02 = -0.2472814;
    constexpr double b12 = 2.4576469;
    constexpr double a21 = -0.8501341;
    constexpr double b30 = -0.8192156;
    constexpr double a03 = -0.0655238;
    constexpr double b31 = -0.0560092;
    constexpr double a22 = -0.0171137;
    constexpr double b13 = 0.0560089;
    constexpr double a40 = 0.0052771;
    constexpr double b32 = -0.0025614;
    constexpr double a23 = -0.0003859;
    constexpr double b14 = 0.0012770;
    constexpr double a41 = 0.0003314;
    constexpr double b50 = 0.0002574;
    constexpr double a04 = 0.0000371;
    constexpr double b33 = -0.0000973;
    constexpr double a42 = 0.0000143;
    constexpr double b51 = 0.0000293;
    constexpr double a24 = -0.0000090;
    constexpr double b15 = 0.0000291;

    double dx = (x - x0) * 1e-5;
    double dy = (y - y0) * 1e-5;

    double df = a01 * dy + a20 * pow(dx, 2) + a02 * pow(dy, 2) + a21 * pow(dx, 2) * dy + a03 * pow(dy, 3);
    df += a40 * pow(dx, 4) + a22 * pow(dx, 2) * pow(dy, 2) + a04 * pow(dy, 4) + a41 * pow(dx, 4) * dy;
    df += a23 * pow(dx, 2) * pow(dy, 3) + a42 * pow(dx, 4) * pow(dy, 2) + a24 * pow(dx, 2) * pow(dy, 4);

    double f = f0 + df / 3600;

    double dl = b10 * dx + b11 * dx * dy + b30 * pow(dx, 3) + b12 * dx * pow(dy, 2) + b31 * pow(dx, 3) * dy;
    dl += b13 * dx * pow(dy, 3) + b50 * pow(dx, 5) + b32 * pow(dx, 3) * pow(dy, 2) + b14 * dx * pow(dy, 4);
    dl += b51 * pow(dx, 5) * dy + b33 * pow(dx, 3) * pow(dy, 3) + b15 * dx * pow(dy, 5);

    double l = l0 + dl / 3600;

    WGS84Pos ret;
    ret.lat = f + (-96.862 - 11.714 * (f - 52) - 0.125 * (l - 5)) / 100000;
    ret.lon = l + (-37.902 + 0.329 * (f - 52) - 14.667 * (l - 5)) / 100000;
    return ret;
};
