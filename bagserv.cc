#include "httplib.h"
#include "sqlwriter.hh"
#include "json.hpp"
#include <iostream>
#include <mutex>

using namespace std;

string packResults(const vector<unordered_map<string,string>>& result)
{
  nlohmann::json arr = nlohmann::json::array();
  
  for(const auto& r : result) {
    nlohmann::json j;
    for(const auto& c : r) {
      if(c.first == "gebruiksdoelen") {
        // parse and store as array
        nlohmann::json ex1 = nlohmann::json::parse(c.second);
        j[c.first]=ex1;
      }
      else 
        j[c.first]=c.second;
    }
    arr += j;
  }
  return arr.dump();
}
                  

int main(int argc, char**argv)
{
  SQLiteWriter sqw("bag.sqlite");
  std::mutex sqwlock;
  // HTTP
  httplib::Server svr;

  svr.set_mount_point("/", "./html/");

    struct LockedSqw
  {
    SQLiteWriter& sqw;
    std::mutex& sqwlock;
    vector<unordered_map<string,string>> query(const std::string& query, const std::initializer_list<SQLiteWriter::var_t>& values)
    {
      std::lock_guard<mutex> l(sqwlock);
      return sqw.query(query, values);
    }
  };
  LockedSqw lsqw{sqw, sqwlock};

  // SQW IS NOT IN ANY WAY THREAD SAFE SO DO NOT FORGET THE LOCK!!

  svr.Get(R"(/(\d\d\d\d[A-Z][A-Z])/(\d+)/([a-zA-Z]))", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    try {
      auto pcode = req.matches[1];
      auto huisnum = req.matches[2];
      auto huisletter = (string)req.matches[3];
      if(!huisletter.empty())
        huisletter[0]=toupper(huisletter[0]);
      cout<<"Query for "<<pcode<<", "<<huisnum<<", "<<huisletter<<endl;
      auto result = lsqw.query("select x as rdX, y as rdY,straat,woonplaats,huisnummer,huisletter,huistoevoeging,oppervlakte,lon,lat,gebruiksdoelen,bouwjaar,num_status,vbo_status from alllabel where postcode=? and huisnummer=? and huisletter=?", {pcode, huisnum,huisletter});

      res.set_content(packResults(result), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });

    svr.Get(R"(/(\d\d\d\d[A-Z][A-Z])/(\d+)/([a-zA-Z])/([a-zA-Z0-9]*))", [&sqw, &sqwlock](const httplib::Request &req, httplib::Response &res) {
    try {
      auto pcode = req.matches[1];
      auto huisnum = req.matches[2];
      auto huisletter = (string)req.matches[3];
      auto huistoevoeging = (string)req.matches[4];
      if(!huisletter.empty())
        huisletter[0]=toupper(huisletter[0]);
      cout<<"Query for "<<pcode<<", "<<huisnum<<", huisletter "<<huisletter<<", huistoevoeging: "<<huistoevoeging<<endl;
      vector<unordered_map<string,string>> result;
      {
        std::lock_guard<mutex> l(sqwlock);
        result=sqw.query("select x as rdX, y as rdY, straat,woonplaats,huisnummer,huisletter,huistoevoeging,oppervlakte,lon,lat,gebruiksdoelen,bouwjaar,num_status,vbo_status from alllabel where postcode=? and huisnummer=? and huisletter=? and huistoevoeging=?", {pcode, huisnum,huisletter,huistoevoeging});
      }

      res.set_content(packResults(result), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });

  
  svr.Get(R"(/(\d\d\d\d[A-Z][A-Z])/(\d+))", [&sqw, &sqwlock](const httplib::Request &req, httplib::Response &res) {
    try {
      auto pcode = req.matches[1];
      auto huisnum = req.matches[2];
      cout<<"Query for "<<pcode<<", "<<huisnum<<endl;
      vector<unordered_map<string,string>> result;
      {
        std::lock_guard<mutex> l(sqwlock);
        result=sqw.query("select x as rdX, y as rdY, straat,woonplaats,huisnummer,huisletter,huistoevoeging,oppervlakte,lon,lat,gebruiksdoelen,bouwjaar,num_status,vbo_status from alllabel where postcode=? and huisnummer=?", {pcode, huisnum});
      }

      res.set_content(packResults(result), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });

  svr.Get(R"(/(\d\d\d\d[A-Z][A-Z]))", [&sqw, &sqwlock](const httplib::Request &req, httplib::Response &res) {
    try {
      auto pcode = req.matches[1];
      cout<<"Query for "<<pcode<<endl;
      vector<unordered_map<string,string>> result;
      {
        std::lock_guard<mutex> l(sqwlock);
        result=sqw.query("select x as rdX, y as rdY,straat,woonplaats,huisnummer,huisletter,huistoevoeging,oppervlakte,bouwjaar,lon,lat,gebruiksdoelen,num_status,vbo_status from alllabel where postcode=?", {pcode});
      }
      res.set_content(packResults(result), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });

  svr.Get(R"(/(\d*\.\d*)/(\d*\.\d*))", [&sqw, &sqwlock](const httplib::Request &req, httplib::Response &res) {
    try {
      // 52.04233332908745, 4.386007891085206
      auto latstr = (string)req.matches[1];  // "y value, > 50"
      auto lonstr = (string)req.matches[2]; // "x value, between 3 and 7"
      double lat = atof(latstr.c_str());
      double lon = atof(lonstr.c_str());
      //      cout<<"lat: "<<lat<<endl;
      // cout<<"lon: "<<lon<<endl;
      cout<<"Query for "<<lat<<", "<<lon<<endl;
      vector<unordered_map<string,string>> result;
      {
        std::lock_guard<mutex> l(sqwlock);
        result=sqw.query("select x as rdX, y as rdY,straat,woonplaats,huisnummer,huisletter,huistoevoeging,oppervlakte,bouwjaar,lon,lat,gebruiksdoelen,num_status,vbo_status, (lat-?)*(lat-?)+(lon-?)*(lon-?) as deg2dist from geoindex,alllabel where alllabel.vbo_id = geoindex.vbo_id and minLat > ? and maxLat < ? and minLon > ? and maxLon < ? order by deg2dist asc limit 1", {lat, lat, lon, lon, lat-0.005, lat+0.005, lon-0.005, lon+0.005}) ;

      }
      res.set_content(packResults(result), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });

  
  
  svr.listen("0.0.0.0", argc==1 ? 8080 : atoi(argv[1]));
}
