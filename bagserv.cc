#define CPPHTTPLIB_USE_POLL
#include "httplib.h"
#include "sqlwriter.hh"
#include "json.hpp"
#include <iostream>
#include <mutex>
#include "jsonhelper.hh"

using namespace std;

void convToHTTP(httplib::Response & res, const std::vector<std::unordered_map<std::string,MiniSQLite::outvar_t>>& content)
{
  res.set_content(packResultsJsonStr(content), "application/json");
}

int main(int argc, char**argv)
{
  httplib::Server svr;
  svr.set_mount_point("/", "./html/");

  svr.Get(R"(/(\d\d\d\d[A-Z][A-Z])/(\d+)/([a-zA-Z]))", [](const httplib::Request &req, httplib::Response &res) {
    SQLiteWriter sqw("bag.sqlite", SQLWFlag::ReadOnly);
    auto pcode = req.matches[1];
    auto huisnum = req.matches[2];
    auto huisletter = (string)req.matches[3];
    if(!huisletter.empty())
      huisletter[0]=toupper(huisletter[0]);
    cout<<"Query for "<<pcode<<", "<<huisnum<<", "<<huisletter<<endl;
    convToHTTP(res, sqw.queryT("select x as rdX, y as rdY,straat,woonplaats,huisnummer,huisletter,huistoevoeging,oppervlakte,lon,lat,gebruiksdoelen,bouwjaar,num_status,vbo_status from alllabel where postcode=? and huisnummer=? and huisletter=?", {pcode, huisnum,huisletter}));

  });
  // with 'toevoeging'
  svr.Get(R"(/(\d\d\d\d[A-Z][A-Z])/(\d+)/([a-zA-Z])/([a-zA-Z0-9]*))", [](const httplib::Request &req, httplib::Response &res) {
    SQLiteWriter sqw("bag.sqlite", SQLWFlag::ReadOnly);
    auto pcode = req.matches[1];
    auto huisnum = req.matches[2];
    auto huisletter = (string)req.matches[3];
    auto huistoevoeging = (string)req.matches[4];
    if(!huisletter.empty())
      huisletter[0]=toupper(huisletter[0]);
    cout<<"Query for "<<pcode<<", "<<huisnum<<", huisletter "<<huisletter<<", huistoevoeging: "<<huistoevoeging<<endl;
    convToHTTP(res, sqw.queryT("select x as rdX, y as rdY, straat,woonplaats,huisnummer,huisletter,huistoevoeging,oppervlakte,lon,lat,gebruiksdoelen,bouwjaar,num_status,vbo_status from alllabel where postcode=? and huisnummer=? and huisletter=? and huistoevoeging=?", {pcode, huisnum,huisletter,huistoevoeging}));
  });

  // postcode/huisnummer
  svr.Get(R"(/(\d\d\d\d[A-Z][A-Z])/(\d+))", [](const httplib::Request &req, httplib::Response &res) {
    SQLiteWriter sqw("bag.sqlite", SQLWFlag::ReadOnly);
    auto pcode = req.matches[1];
    auto huisnum = req.matches[2];
    cout<<"Query for "<<pcode<<", "<<huisnum<<endl;
    convToHTTP(res, sqw.queryT("select x as rdX, y as rdY, straat,woonplaats,huisnummer,huisletter,huistoevoeging,oppervlakte,lon,lat,gebruiksdoelen,bouwjaar,num_status,vbo_status from alllabel where postcode=? and huisnummer=?", {pcode, huisnum}));
  });

  // postcode
  svr.Get(R"(/(\d\d\d\d[A-Z][A-Z]))", [](const httplib::Request &req, httplib::Response &res) {
    SQLiteWriter sqw("bag.sqlite", SQLWFlag::ReadOnly);
    auto pcode = req.matches[1];
    cout<<"Query for "<<pcode<<endl;
    
    convToHTTP(res, sqw.queryT("select x as rdX, y as rdY,straat,woonplaats,huisnummer,huisletter,huistoevoeging,oppervlakte,bouwjaar,lon,lat,gebruiksdoelen,num_status,vbo_status from alllabel where postcode=?", {pcode}));
  });

  // coordinates
  svr.Get(R"(/(\d*\.\d*)/(\d*\.\d*))", [](const httplib::Request &req, httplib::Response &res) {
    SQLiteWriter sqw("bag.sqlite", SQLWFlag::ReadOnly);
    // 52.04233332908745, 4.386007891085206
    auto latstr = (string)req.matches[1];  // "y value, > 50"
    auto lonstr = (string)req.matches[2]; // "x value, between 3 and 7"
    double lat = atof(latstr.c_str());
    double lon = atof(lonstr.c_str());
    //      cout<<"lat: "<<lat<<endl;
    // cout<<"lon: "<<lon<<endl;
    cout<<"Query for "<<lat<<", "<<lon<<endl;
    convToHTTP(res, sqw.queryT("select x as rdX, y as rdY,straat,woonplaats,huisnummer,huisletter,huistoevoeging,postcode,oppervlakte,bouwjaar,lon,lat,gebruiksdoelen,num_status,vbo_status, (lat-?)*(lat-?)+(lon-?)*(lon-?) as deg2dist from geoindex,alllabel where alllabel.vbo_id = geoindex.vbo_id and minLat > ? and maxLat < ? and minLon > ? and maxLon < ? order by deg2dist asc limit 1", {lat, lat, lon, lon, lat-0.005, lat+0.005, lon-0.005, lon+0.005})) ;
  });

  /*
  svr.Get(R"(/street-partial/:street)", [](const httplib::Request &req, httplib::Response &res) {
    string street = req.path_params.at("street");
    street = "%"+street+"%";
    cout<<"Searching for '"<<street<<"'\n";
    lsqw.queryJ(res, "select oprs.id,naam,woonplaats,count(1) as 'count' from oprs,nums where naam like ? and oprs.id==nums.ligtAanRef group by oprs.id order by 4 desc limit 10", {street});
  });

  svr.Get(R"(/street-partial2/:street)", [](const httplib::Request &req, httplib::Response &res) {
    string street = req.path_params.at("street");
    cout<<"Searching for '"<<street<<"'\n";
    lsqw.queryJ(res, "select oprs.id,naam,woonplaats,count(1) as 'count' from tri(?),oprs,nums where oprs.naam=a and oprs.id==nums.ligtAanRef group by oprs.id order by 4 desc limit 10", {street});
  });
  */
  
  int port = argc==1 ? 8080 : atoi(argv[1]);
  cout<<"Will listen on http://127.0.0.1:"<<port<<endl;
  
  svr.listen("0.0.0.0", port);
}
