#include "pugixml.hpp"
#include <iostream>
#include <map>
#include <unordered_map>
#include <unistd.h>
#include <set>
#include "sqlwriter.hh"
#include "rd2wgs84.hh"
#include "json.hpp"
using namespace std;

/* 
   Woonplaats names are in WPL
   hierarchy: sl:standBestand, sl:stand sl-bag-extract:bagObject Objecten:Woonplaats 
     Objecten:identificatie has the number
     1245 = 's-Gravenhage

   street names are in OPR
     Objecten:identificatie is the key & link to NUM.
     Objecten:naam has name
     Has a link to Objecten:ligtIn, which has a WoonplaatsRef value
   hierarchy: sl:standBestand, sl:stand, sl-bag-extract:bagObject, Objecten:OpenbareRuimte

   NUM has Nummeraanduidingen, which have a street number, postcode and a 
     'ligtAan' reference to a street ('OpenbareRuimteRef').
     hierarchy: sl:stand, sl-bag-extract:bagObject, Objecten:Nummeraanduiding

     COULD HAVE A ligtIn (if it lies in another woonplaats than the openbareruimte)

   VBO, verblijfsobject. Has a reference to a Nummeraanduiding through heeftAlshoofdadres. Also has an ID itself. Contains a point location, an area, a gebruiksdoel. It also has a reference to a Pand (PND) through maaktDeelUitVan.
   Also references related addresses (Nummeraanduiding) through heeftAlsNevenAdres.

   PND, pand or building, has date of construction, plus a shape file of the exterior. Also a status.
*/

/* <Historie:beginGeldigheid>2009-07-31</Historie:beginGeldigheid>
   <Historie:eindGeldigheid>2018-01-01</Historie:eindGeldigheid>

   or

   <Historie:beginGeldigheid>2018-01-01</Historie:beginGeldigheid>
*/



// date yyyy-mm-dd
bool currentlyActive(const pugi::xml_node& stand, const string& date, string* enddate=0, string* begindate=0)
{
  if(enddate)
    enddate->clear();
  if(begindate)
    begindate->clear();
  if(auto iter = stand.child("Objecten:voorkomen").child("Historie:Voorkomen"); iter) {
    string begin, end;
    if(auto enditer = iter.child("Historie:eindGeldigheid")) {
      end = enditer.begin()->value();
    }
    if(auto begiter = iter.child("Historie:beginGeldigheid")) {
      begin = begiter.begin()->value();
    }
    if(begin.empty()) {
      cout<<stand<<endl;
      cout<<"begin empty?!"<<endl;
      abort();
    }
    if(begindate)
      *begindate = begin;
    if(enddate)
      *enddate = end;
          
    if(date < begin) {// not yet valid
      //      cout<<"Not yet active, "<<date <<" < "<<begin<<"\n";
      return false;
    }
    if(end.empty()) // no end date, so still valid
      return true;
    if(end <= date) {// this interval expired already
      //      cout<<"Was active from "<<begin<<" to "<<end<<", and that period expired already"<<endl;
      return false;
    }
    if(!begin.empty() && !end.empty()) {
      //      cout<<"Still active, but end is known, "<<begin << " < "<<date<<" < "<< end << endl;
      // cout<<stand<<endl;
      return true;
    }
  }

  return true;
}

static string getYYYYMMDD()
{
  time_t now = time(0);
  struct tm tm;
  localtime_r(&now, &tm);
  char tmp[80];
  strftime(tmp, sizeof(tmp), "%Y-%m-%d", &tm);
  return tmp;
}

// written by ChatGPT!
vector<string> split_string(const string& input)
{
  istringstream iss(input);
  vector<string> tokens;
  string token;
  while (iss >> token)
    tokens.push_back(token);
  return tokens;
}


int main(int argc, char **argv)
{
  unlink("bag.sqlite");
  SQLiteWriter sqw("bag.sqlite", {{"huisletter", "collate nocase"}, {"huistoevoeging", "collate nocase"}});
  unordered_map<int, string> wpls;

  string today=getYYYYMMDD(); // you can override this for limited timetravel
  cout<<"Reference day for history: "<<today<<endl;
  struct OpenbareRuimte
  {
    int ligtIn;
    string naam;
    string verkorteNaam;
    string status;
    string type;
  };
  unordered_map<string, OpenbareRuimte> oprs;

  struct VboNumKoppeling
  {
    string numid;
    bool hoofdadres;
    bool operator<(const VboNumKoppeling& rhs) const
    {
      return std::tie(numid, hoofdadres) < std::tie(rhs.numid, rhs.hoofdadres);
    }
  };
  
  struct VerblijfsObject
  {
    int oppervlakte{-1};
    string status;
    set<string> panden;
    set<VboNumKoppeling> nums;
    set<string> gebruiksdoelen;
    string type;
    double x{-1}, y{-1};
  };
  unordered_map<string, VerblijfsObject> vbos;
  struct NummerAanduiding
  {
    int huisnummer;
    string huisletter;
    string toevoeging;
    string ligtAan;
    int ligtIn{-1};
    set<string> vbos; // free dedup
    string postcode;
    string status;
  };
  std::unordered_map<string, NummerAanduiding> nums;

  struct Pand
  {
    int constructionYear;
    string status;
    string geo;
  };
  std::unordered_map<string, Pand> pnds;
  
  unsigned int count=0;
  map<pair<string, string>, int> once;
  for(int n=1; n< argc; ++n) {
    pugi::xml_document doc;
    if (!doc.load_file(argv[n])) return -1;

    auto start = doc.child("sl-bag-extract:bagStand").child("sl:standBestand");

    
    for(const auto& node : start) {
      const auto& deeper = node.child("sl-bag-extract:bagObject");
      for(const auto& stand : deeper) {
        string type=stand.name();
        count++;
        if(!(count % (32768*32)))
          cout<<count<<endl;
        if(type=="Objecten:Woonplaats") {
          string enddate;
          if(!currentlyActive(stand, today, &enddate))
            continue;
          
          int id; 
          string name;
          bool geconstateerd=false;

          for(const auto& el : stand) {
            string elname = el.name();
            if(elname=="Objecten:identificatie")
              id = atoi(el.begin()->value());
            else if(elname=="Objecten:naam")
              name = el.begin()->value();
            else if(elname=="Objecten:geconstateerd")
              geconstateerd = string(el.begin()->value())=="J";
            else if(elname=="Objecten:geometrie" || elname=="Objecten:voorkomen")
              ;
            else if(!once[{type, elname}]++) {
              cout<<"Ignoring "<<type<<" element "<<elname<<endl;
            }
          }

          
          wpls[id]=name;
          sqw.addValue({{"id", id}, {"naam", name}, {"geconstateerd", geconstateerd}}, "wpls");
        }
        else if(type=="Objecten:Pand") {
          string enddate;
          if(!currentlyActive(stand, today, &enddate))
            continue;

          Pand pnd;
          string id;

          for(const auto& el : stand) {
            string elname = el.name();
            if(elname=="Objecten:identificatie")
              id = el.begin()->value();
            else if(elname=="Objecten:status")
              pnd.status = el.begin()->value();
            else if(elname=="Objecten:oorspronkelijkBouwjaar") 
              pnd.constructionYear = atoi(el.begin()->value());
            else if(elname=="Objecten:geometrie") {
              auto pnode=el.select_nodes("gml:Polygon/gml:exterior/gml:LinearRing/gml:posList");
              
              if(pnode.begin() != pnode.end()) {
                pnd.geo = pnode.begin()->node().begin()->value();
              }
            }
            else if(elname=="Objecten:geometrie" || elname=="Objecten:voorkomen")
              ;
            else if(!once[{type, elname}]++) {
              cout<<"Ignoring "<<type<<" element "<<elname<<endl;
            }
          }
          // possibly store enddate here
          sqw.addValue({{"id", id}, {"geo", pnd.geo}, {"bouwjaar", pnd.constructionYear}, {"status", pnd.status}}, "pnds");
        }
        else if(type=="Objecten:Verblijfsobject" || type=="Objecten:Standplaats" || type=="Objecten:Ligplaats") {
          string enddate;
          if(!currentlyActive(stand, today, &enddate))
            continue;

          VerblijfsObject vo;
          string id;
          for(const auto& el : stand) {
            string elname = el.name();
            if(elname=="Objecten:identificatie")
              id = el.begin()->value();
            else if(elname=="Objecten:status")
              vo.status = el.begin()->value();
            else if(elname=="Objecten:oppervlakte") 
              vo.oppervlakte = atoi(el.begin()->value());
            else if(elname=="Objecten:gebruiksdoel")  
              vo.gebruiksdoelen.insert(el.begin()->value());
            else if(elname=="Objecten:geometrie") {
              auto pnode=el.select_nodes("Objecten:punt/gml:Point/gml:pos");
              if(pnode.begin() != pnode.end()) {
                string point = pnode.begin()->node().begin()->value();
                vo.x = atof(point.c_str());
                if(auto pos = point.find(' '); pos != string::npos)
                  vo.y = atof(point.c_str() + pos + 1);
              }
              else {
                pnode=el.select_nodes("gml:Polygon/gml:exterior/gml:LinearRing/gml:posList");
                if(pnode.begin() != pnode.end()) {
                  string poslist = pnode.begin()->node().begin()->value();
                  auto s = split_string(poslist);
                  if(s.size() % 2) {
                    cout<<"Odd size for position list in ring"<<endl;
                  }
                  else {
                    vo.x = vo.y = 0;
                    for(unsigned int pos = 0 ; pos < s.size() ; pos += 2) {
                      vo.x += atof(s.at(pos).c_str());
                      vo.y += atof(s.at(pos+1).c_str());
                    }
                    vo.x /= (s.size() / 2.0);
                    vo.y /= (s.size() / 2.0);
                  }
                }
              }
              /* 
<gml:Polygon srsName="urn:ogc:def:crs:EPSG::28992" srsDimension="2">
  <gml:exterior>
    <gml:LinearRing>
      <gml:posList count="8">252451.865 593746.633 252450.808 593748.894 252431.9 593740.053 252432.842 593737.739 252433.901 593735.474 252435.595 593731.85 252454.618 593740.745 252451.865 593746.633</gml:posList>
    </gml:LinearRing>
  </gml:exterior>
</gml:Polygon>
              */
              
            }
            else if(elname=="Objecten:maaktDeelUitVan") {
              if(auto iter = el; iter) {
                int n=0;
                for(const auto& el : iter) {
                  vo.panden.insert(el.begin()->value());
                  ++n;
                }
              }
            }
            // refers to a Nummeraanduiding
            else if(elname=="Objecten:heeftAlsHoofdadres") {
              int n=0;
              for(const auto& el2 : el) {
                vo.nums.insert({el2.begin()->value(), true});
                ++n;
              }
            }
            // refers to further Nummeraanduidingen
            else if(elname=="Objecten:heeftAlsNevenadres") {
              int n=0;
              for(const auto& el2 : el) {
                vo.nums.insert({el2.begin()->value(), false});
                ++n;
              }
            }
            else if(!once[{type, elname}]++) {
              cout<<"Ignoring "<<type<<" element "<<elname<<endl;
            }
          }          
          
          if(type=="Objecten:Verblijfsobject")
            vo.type="vbo";
          else if(type=="Objecten:Standplaats")
            vo.type="sta";
          else if(type=="Objecten:Ligplaats")
            vo.type="lig";

          // possibly store enddate
          WGS84Pos wpos = rd2wgs84(vo.x, vo.y);
          nlohmann::json arr = nlohmann::json::array();
          for(const auto& g : vo.gebruiksdoelen) {
            arr += g;
          }
          string gebruiksdoelen = arr.dump();
          sqw.addValue({{"id", id}, {"gebruiksdoelen", gebruiksdoelen},
                        {"x", vo.x}, {"y", vo.y}, {"lat", wpos.lat}, {"lon", wpos.lon}, {"status", vo.status},
                        {"oppervlakte", vo.oppervlakte}, {"type", vo.type}}, "vbos");

          for(const auto& num : vo.nums) {
            sqw.addValue({{"vbo", id}, {"num", num.numid}, {"hoofdadres", num.hoofdadres}}, "vbo_num");
          }
          for(const auto& pnd : vo.panden) {
            sqw.addValue({{"vbo", id}, {"pnd", pnd}}, "vbo_pnd");
          }
        }
        else if(type=="Objecten:Nummeraanduiding") {
          string enddate, begindate;

          NummerAanduiding na;
          string woonplaats;
          string id;
          for(const auto& el : stand) {
            string elname = el.name();
            if(elname=="Objecten:identificatie")
              id = el.begin()->value();
            else if(elname=="Objecten:status")
              na.status = el.begin()->value();
            else if(elname=="Objecten:huisnummer")
              na.huisnummer = atoi(el.begin()->value());
            else if(elname=="Objecten:huisletter")
              na.huisletter = el.begin()->value();
            else if(elname=="Objecten:huisnummertoevoeging")
              na.toevoeging = el.begin()->value();
            else if(elname=="Objecten:postcode")
              na.postcode = el.begin()->value();
            else if(elname=="Objecten:ligtAan")
              na.ligtAan = el.child("Objecten-ref:OpenbareRuimteRef").begin()->value();
            else if(elname=="Objecten:ligtIn") {
              na.ligtIn = atoi(el.child("Objecten-ref:WoonplaatsRef").begin()->value());
              woonplaats=wpls[na.ligtIn];
            }
            else if(!once[{type, elname}]++) {
              cout<<"Ignoring "<<type<<" element "<<elname<<": "<<el.begin()->value()<<endl;
            }
          }
          if(woonplaats.empty())
            woonplaats=wpls[oprs[na.ligtAan].ligtIn];
            
          if(woonplaats.empty()) {
            cout<<na.ligtAan<<endl;
            cout<<oprs[na.ligtAan].ligtIn<<endl;
            cout<<"Failure to look up woonplaats for Nummeraanduiding id "<<id<<", make sure to parse WPL and OPR files first!"<<endl;
            exit(1);
          }

          if(!currentlyActive(stand, today, &enddate, &begindate)) {
            if(na.ligtIn >=0)
              sqw.addValue({{"id", id}, {"ligtAanRef", na.ligtAan}, {"ligtInRef", na.ligtIn}, {"woonplaats", woonplaats}, {"postcode", na.postcode}, {"huisnummer", na.huisnummer}, {"huisletter", na.huisletter}, {"huistoevoeging", na.toevoeging}, {"status", na.status}, {"begindate", begindate}, {"enddate", enddate}}, "inactnums");
          else
            sqw.addValue({{"id", id}, {"ligtAanRef", na.ligtAan}, {"woonplaats", woonplaats}, {"postcode", na.postcode}, {"huisnummer", na.huisnummer}, {"huisletter", na.huisletter}, {"huistoevoeging", na.toevoeging}, {"status", na.status}, {"begindate", begindate}, {"enddate", enddate}}, "inactnums");

          }
          else {
          // possibly store enddate
          if(na.ligtIn >=0)
            sqw.addValue({{"id", id}, {"ligtAanRef", na.ligtAan}, {"ligtInRef", na.ligtIn}, {"woonplaats", woonplaats}, {"postcode", na.postcode}, {"huisnummer", na.huisnummer}, {"huisletter", na.huisletter}, {"huistoevoeging", na.toevoeging}, {"status", na.status}}, "nums");
          else
            sqw.addValue({{"id", id}, {"ligtAanRef", na.ligtAan}, {"woonplaats", woonplaats}, {"postcode", na.postcode}, {"huisnummer", na.huisnummer}, {"huisletter", na.huisletter}, {"huistoevoeging", na.toevoeging}, {"status", na.status}}, "nums");
          }
        }
        else if(type=="Objecten:OpenbareRuimte") {
          string enddate;
          if(!currentlyActive(stand, today, &enddate))
            continue;
          OpenbareRuimte opr;
          string id;
          for(const auto& el : stand) {
            string elname = el.name();
            if(elname=="Objecten:identificatie")
              id = el.begin()->value();
            else if(elname=="Objecten:status")
              opr.status = el.begin()->value();
            else if(elname=="Objecten:naam")
              opr.naam = el.begin()->value();
            else if(elname=="Objecten:type")
              opr.type = el.begin()->value();
            else if(elname=="Objecten:ligtIn")
              opr.ligtIn = atoi(el.child("Objecten-ref:WoonplaatsRef").begin()->value());
            else if(elname=="Objecten:verkorteNaam") {
              auto pnode=el.select_nodes("nen5825:VerkorteNaamOpenbareRuimte/nen5825:verkorteNaam");
              if(pnode.begin() != pnode.end()) {
                opr.verkorteNaam=pnode.begin()->node().begin()->value();
              }
            }
            else if(!once[{type, elname}]++) {
              cout<<"Ignoring "<<type<<" element "<<elname<<endl;
            }
          }

          // possibly store enddate here
          if(!opr.verkorteNaam.empty()) 
            sqw.addValue({{"id", id}, {"naam", opr.naam}, {"verkorteNaam", opr.verkorteNaam}, {"type", opr.type}, {"status", opr.status}, {"ligtInRef", opr.ligtIn}}, "oprs");
          else
            sqw.addValue({{"id", id}, {"naam", opr.naam}, {"type", opr.type}, {"status", opr.status}, {"ligtInRef", opr.ligtIn}}, "oprs");
          
          oprs[id]=opr;
        }
        else
          cout<<stand.name() << '\n';
      }
    }
  }
}

