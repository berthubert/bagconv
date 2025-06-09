#define CPPHTTPLIB_USE_POLL
#define CPPHTTPLIB_THREAD_POOL_COUNT 32

#include "httplib.h"
#include "sqlwriter.hh"
#include "json.hpp"
#include <iostream>
#include <mutex>
#include "thingpool.hh"
#include "jsonhelper.hh"

using namespace std;

struct DTime
{
  DTime()
  {
    d_start =   std::chrono::steady_clock::now();
  }
  uint32_t usec()
  {
    auto usec = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()- d_start).count();
    return usec;
  }

  std::chrono::time_point<std::chrono::steady_clock> d_start;
};


static void convToHTTP(httplib::Response & res, const std::vector<std::unordered_map<std::string,MiniSQLite::outvar_t>>& content)
{
  auto j = packResultsJson(content);
  for(auto& r : j) {
    if(r.count("gebruiksdoelen")) {
      r["gebruiksdoelen"] = nlohmann::json::parse((string)r["gebruiksdoelen"]);
    }
  }
  res.set_content(j.dump(), "application/json");
}

int main(int argc, char**argv)
{
  httplib::Server svr;
  svr.set_mount_point("/", "./html/");
  svr.set_keep_alive_max_count(1); // Default is 5
  svr.set_keep_alive_timeout(1);  // Default is 5
  ThingPool<SQLiteWriter> tp("bag.sqlite", SQLWFlag::ReadOnly);
  
  svr.Get(R"(/(\d\d\d\d[A-Z][A-Z])/(\d+)/([a-zA-Z]))", [&tp](const httplib::Request &req, httplib::Response &res) {
    auto pcode = req.matches[1];
    auto huisnum = req.matches[2];
    auto huisletter = (string)req.matches[3];
    if(!huisletter.empty())
      huisletter[0]=toupper(huisletter[0]);

    convToHTTP(res, tp.getLease()->queryT("select x as rdX, y as rdY,straat,woonplaats,huisnummer,huisletter,huistoevoeging,oppervlakte,lon,lat,gebruiksdoelen,bouwjaar,num_status,vbo_status from alllabel where postcode=? and huisnummer=? and huisletter=?", {pcode, huisnum,huisletter}));
  });

  // with 'toevoeging'
  svr.Get(R"(/(\d\d\d\d[A-Z][A-Z])/(\d+)/([a-zA-Z]?)/([a-zA-Z0-9]*))", [&tp](const httplib::Request &req, httplib::Response &res) {
    auto pcode = req.matches[1];
    auto huisnum = req.matches[2];
    auto huisletter = (string)req.matches[3];
    auto huistoevoeging = (string)req.matches[4];
    if(!huisletter.empty())
      huisletter[0]=toupper(huisletter[0]);
    convToHTTP(res, tp.getLease()->queryT("select x as rdX, y as rdY, straat,woonplaats,huisnummer,huisletter,huistoevoeging,oppervlakte,lon,lat,gebruiksdoelen,bouwjaar,num_status,vbo_status from alllabel where postcode=? and huisnummer=? and huisletter=? and huistoevoeging=?", {pcode, huisnum,huisletter,huistoevoeging}));
  });

  // postcode/huisnummer
  svr.Get(R"(/(\d\d\d\d[A-Z][A-Z])/(\d+))", [&tp](const httplib::Request &req, httplib::Response &res) {
    auto pcode = req.matches[1];
    auto huisnum = req.matches[2];

    convToHTTP(res, tp.getLease()->queryT("select x as rdX, y as rdY, straat,woonplaats,huisnummer,huisletter,huistoevoeging,oppervlakte,lon,lat,gebruiksdoelen,bouwjaar,num_status,vbo_status from alllabel where postcode=? and huisnummer=?", {pcode, huisnum}));
  });

  // postcode
  svr.Get(R"(/(\d\d\d\d[A-Z][A-Z]))", [&tp](const httplib::Request &req, httplib::Response &res) {
    auto pcode = req.matches[1];
    convToHTTP(res, tp.getLease()->queryT("select x as rdX, y as rdY,straat,woonplaats,huisnummer,huisletter,huistoevoeging,oppervlakte,bouwjaar,lon,lat,gebruiksdoelen,num_status,vbo_status from alllabel where postcode=?", {pcode}));
  });

  // coordinates
  svr.Get(R"(/(\d*\.\d*)/(\d*\.\d*))", [&tp](const httplib::Request &req, httplib::Response &res) {
    // 52.04233332908745/4.386007891085206
    auto latstr = (string)req.matches[1];  // "y value, > 50"
    auto lonstr = (string)req.matches[2]; // "x value, between 3 and 7"
    double lat = atof(latstr.c_str());
    double lon = atof(lonstr.c_str());
    convToHTTP(res, tp.getLease()->queryT("select x as rdX, y as rdY,straat,woonplaats,huisnummer,huisletter,huistoevoeging,postcode,oppervlakte,bouwjaar,lon,lat,gebruiksdoelen,num_status,vbo_status, (lat-?)*(lat-?)+(lon-?)*(lon-?) as deg2dist from geoindex,alllabel where alllabel.vbo_id = geoindex.vbo_id and minLat > ? and maxLat < ? and minLon > ? and maxLon < ? order by deg2dist asc limit 1", {lat, lat, lon, lon, lat-0.005, lat+0.005, lon-0.005, lon+0.005})) ;
  });

  svr.set_exception_handler([](const auto&, auto& res, std::exception_ptr ep) {
    auto fmt = "<h1>Error 500</h1><p>%s</p>";
    char buf[BUFSIZ];
    try {
      std::rethrow_exception(ep);
    } catch (std::exception &e) {
      snprintf(buf, sizeof(buf), fmt, e.what());
    } catch (...) { // See the following NOTE
      snprintf(buf, sizeof(buf), fmt, "Unknown Exception");
    }
    cout<<"Error: '"<<buf<<"'"<<endl;
    res.set_content(buf, "text/html");
    res.status = 500; 
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
  /*
  svr.set_logger([](const auto& req, const auto&) {
    fmt::print("Request: {} {}\n", req.path, req.params);
  });
  */
  
  int port = argc==1 ? 8080 : atoi(argv[1]);
  cout<<"Will listen on http://127.0.0.1:"<<port<< " using "<< CPPHTTPLIB_THREAD_POOL_COUNT << " threads\n";

  svr.set_socket_options([](socket_t sock) {
    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const void *>(&yes), sizeof(yes));
  });

  
  svr.listen("0.0.0.0", port);

  cout <<"Exiting (somehow)" << endl;
}
