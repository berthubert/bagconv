#define CPPHTTPLIB_USE_POLL
#include "httplib.h"
#include "sqlwriter.hh"
#include "json.hpp"
#include <iostream>
#include <mutex>
#include "jsonhelper.hh"

using namespace std;

int main(int argc, char**argv)
{
  SQLiteWriter sqw("bag.sqlite");
  std::mutex sqwlock;

  httplib::Server svr;

  svr.set_mount_point("/", "./html/");

  struct LockedSqw
  {
    SQLiteWriter& sqw;
    std::mutex& sqwlock;
    vector<unordered_map<string, MiniSQLite::outvar_t>> query(const std::string& query, const std::initializer_list<SQLiteWriter::var_t>& values)
    {
      std::lock_guard<mutex> l(sqwlock);
      return sqw.queryT(query, values);
    }

    void queryJ(httplib::Response &res, const std::string& q, const std::initializer_list<SQLiteWriter::var_t>& values) 
    try
    {
      auto result = query(q, values);
      res.set_content(packResultsJsonStr(result), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  };
  LockedSqw lsqw{sqw, sqwlock};

  svr.Get(R"(/(\d\d\d\d[A-Z][A-Z])/(\d+)/([a-zA-Z]))", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    auto pcode = req.matches[1];
    auto huisnum = req.matches[2];
    auto huisletter = (string)req.matches[3];
    if(!huisletter.empty())
      huisletter[0]=toupper(huisletter[0]);
    cout<<"Query for "<<pcode<<", "<<huisnum<<", "<<huisletter<<endl;
    lsqw.queryJ(res, "select x as rdX, y as rdY,straat,woonplaats,huisnummer,huisletter,huistoevoeging,oppervlakte,lon,lat,gebruiksdoelen,bouwjaar,num_status,vbo_status from alllabel where postcode=? and huisnummer=? and huisletter=?", {pcode, huisnum,huisletter});
  });

  svr.Get(R"(/(\d\d\d\d[A-Z][A-Z])/(\d+)/([a-zA-Z])/([a-zA-Z0-9]*))", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    auto pcode = req.matches[1];
    auto huisnum = req.matches[2];
    auto huisletter = (string)req.matches[3];
    auto huistoevoeging = (string)req.matches[4];
    if(!huisletter.empty())
      huisletter[0]=toupper(huisletter[0]);
    cout<<"Query for "<<pcode<<", "<<huisnum<<", huisletter "<<huisletter<<", huistoevoeging: "<<huistoevoeging<<endl;
    lsqw.queryJ(res, "select x as rdX, y as rdY, straat,woonplaats,huisnummer,huisletter,huistoevoeging,oppervlakte,lon,lat,gebruiksdoelen,bouwjaar,num_status,vbo_status from alllabel where postcode=? and huisnummer=? and huisletter=? and huistoevoeging=?", {pcode, huisnum,huisletter,huistoevoeging});
  });

  
  svr.Get(R"(/(\d\d\d\d[A-Z][A-Z])/(\d+))", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    auto pcode = req.matches[1];
    auto huisnum = req.matches[2];
    cout<<"Query for "<<pcode<<", "<<huisnum<<endl;
    lsqw.queryJ(res, "select x as rdX, y as rdY, straat,woonplaats,huisnummer,huisletter,huistoevoeging,oppervlakte,lon,lat,gebruiksdoelen,bouwjaar,num_status,vbo_status from alllabel where postcode=? and huisnummer=?", {pcode, huisnum});
  });

  svr.Get(R"(/(\d\d\d\d[A-Z][A-Z]))", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    auto pcode = req.matches[1];
    cout<<"Query for "<<pcode<<endl;
    
    lsqw.queryJ(res, "select x as rdX, y as rdY,straat,woonplaats,huisnummer,huisletter,huistoevoeging,oppervlakte,bouwjaar,lon,lat,gebruiksdoelen,num_status,vbo_status from alllabel where postcode=?", {pcode});
  });

  svr.Get(R"(/(\d*\.\d*)/(\d*\.\d*))", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    // 52.04233332908745, 4.386007891085206
    auto latstr = (string)req.matches[1];  // "y value, > 50"
    auto lonstr = (string)req.matches[2]; // "x value, between 3 and 7"
    double lat = atof(latstr.c_str());
    double lon = atof(lonstr.c_str());
    //      cout<<"lat: "<<lat<<endl;
    // cout<<"lon: "<<lon<<endl;
    cout<<"Query for "<<lat<<", "<<lon<<endl;
    lsqw.queryJ(res, "select x as rdX, y as rdY,straat,woonplaats,huisnummer,huisletter,huistoevoeging,postcode,oppervlakte,bouwjaar,lon,lat,gebruiksdoelen,num_status,vbo_status, (lat-?)*(lat-?)+(lon-?)*(lon-?) as deg2dist from geoindex,alllabel where alllabel.vbo_id = geoindex.vbo_id and minLat > ? and maxLat < ? and minLon > ? and maxLon < ? order by deg2dist asc limit 1", {lat, lat, lon, lon, lat-0.005, lat+0.005, lon-0.005, lon+0.005}) ;
  });

  /*
  svr.Get(R"(/street-partial/:street)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    string street = req.path_params.at("street");
    street = "%"+street+"%";
    cout<<"Searching for '"<<street<<"'\n";
    lsqw.queryJ(res, "select oprs.id,naam,woonplaats,count(1) as 'count' from oprs,nums where naam like ? and oprs.id==nums.ligtAanRef group by oprs.id order by 4 desc limit 10", {street});
  });

  svr.Get(R"(/street-partial2/:street)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    string street = req.path_params.at("street");
    cout<<"Searching for '"<<street<<"'\n";
    lsqw.queryJ(res, "select oprs.id,naam,woonplaats,count(1) as 'count' from tri(?),oprs,nums where oprs.naam=a and oprs.id==nums.ligtAanRef group by oprs.id order by 4 desc limit 10", {street});
  });
  */
  
  int port = argc==1 ? 8080 : atoi(argv[1]);
  cout<<"Will listen on http://127.0.0.1:"<<port<<endl;
  
  svr.listen("0.0.0.0", port);
}
