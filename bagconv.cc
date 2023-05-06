#include "pugixml.hpp"
#include <iostream>
#include <map>
#include <unordered_map>
#include <unistd.h>
#include <set>
#include "sqlwriter.hh"
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

bool outdated(const pugi::xml_node& stand)
{
  if(auto iter = stand.child("Objecten:voorkomen").child("Historie:Voorkomen"); iter) {
    if(iter.child("Historie:eindGeldigheid")) {
      return true;
    }
  }
  return false;
}

int main(int argc, char **argv)
{
  unlink("bag.sqlite");
  SQLiteWriter sqw("bag.sqlite");
  unordered_map<int, string> wpls;

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
    string gebruiksdoel;
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
  for(int n=1; n< argc; ++n) {
    pugi::xml_document doc;
    if (!doc.load_file(argv[n])) return -1;

    auto start = doc.child("sl-bag-extract:bagStand").child("sl:standBestand");

    for(const auto& node : start) {
      const auto& deeper = node.child("sl-bag-extract:bagObject");
      for(const auto& stand : deeper) {
        string type=stand.name();
        count++;
        if(!(count % 32768))
          cout<<count<<endl;
        if(type=="Objecten:Woonplaats") {
          if(outdated(stand))
            continue;

          int id = atoi(stand.child("Objecten:identificatie").begin()->value());
          string name = stand.child("Objecten:naam").begin()->value();
          wpls[id]=name;
          sqw.addValue({{"id", id}, {"naam", name}}, "wpls");
        }
        else if(type=="Objecten:Pand") {
          if(outdated(stand))
            continue;
          Pand pnd;
          string id = stand.child("Objecten:identificatie").begin()->value();
          pnd.status = stand.child("Objecten:status").begin()->value();
          pnd.constructionYear = atoi(stand.child("Objecten:oorspronkelijkBouwjaar").begin()->value());
          auto pnode=stand.select_nodes("Objecten:geometrie/gml:Polygon/gml:exterior/gml:LinearRing/gml:posList");
          
          if(pnode.begin() != pnode.end()) {
            pnd.geo = pnode.begin()->node().begin()->value();
          }
          sqw.addValue({{"id", id}, {"geo", pnd.geo}, {"bouwjaar", pnd.constructionYear}, {"status", pnd.status}}, "pnds");
          //          pnds[id]=pnd;
          
        }
        else if(type=="Objecten:Verblijfsobject" || type=="Objecten:Standplaats" || type=="Objecten:Ligplaats") {
          if(outdated(stand))
            continue;

          VerblijfsObject vo;
          if(type=="Objecten:Verblijfsobject")
            vo.type="vbo";
          else if(type=="Objecten:Standplaats")
            vo.type="sta";
          else if(type=="Objecten:Ligplaats")
            vo.type="lig";
              
          string id = stand.child("Objecten:identificatie").begin()->value();
          vo.status = stand.child("Objecten:status").begin()->value();
          
          if(stand.child("Objecten:oppervlakte"))
            vo.oppervlakte = atoi(stand.child("Objecten:oppervlakte").begin()->value());
          if(stand.child("Objecten:gebruiksdoel"))
            vo.gebruiksdoel = stand.child("Objecten:gebruiksdoel").begin()->value();

          auto pnode=stand.select_nodes("Objecten:geometrie/Objecten:punt/gml:Point/gml:pos");
          if(pnode.begin() != pnode.end()) {
            string point = pnode.begin()->node().begin()->value();
            vo.x = atof(point.c_str());
            if(auto pos = point.find(' '); pos != string::npos)
              vo.y = atof(point.c_str() + pos + 1);
          }
          
          if(auto iter = stand.child("Objecten:maaktDeelUitVan"); iter) {
            int n=0;
            for(const auto& el : iter) {
              vo.panden.insert(el.begin()->value());
              ++n;
            }
          }

          // refers to a Nummeraanduiding
          if(auto iter = stand.child("Objecten:heeftAlsHoofdadres"); iter) {
            int n=0;
            for(const auto& el : iter) {
              vo.nums.insert({el.begin()->value(), true});
              ++n;
            }
          }
          // refers to further Nummeraanduidingen
          if(auto iter = stand.child("Objecten:heeftAlsNevenadres"); iter) {
            int n=0;
            for(const auto& el : iter) {
              vo.nums.insert({el.begin()->value(), false});
              ++n;
            }
          }
          sqw.addValue({{"id", id}, {"gebruiksdoel", vo.gebruiksdoel},
                        {"x", vo.x}, {"y", vo.y}, {"status", vo.status},
                        {"oppervlakte", vo.oppervlakte}, {"type", vo.type}}, "vbos");

          for(const auto& num : vo.nums) {
            sqw.addValue({{"vbo", id}, {"num", num.numid}, {"hoofdadres", num.hoofdadres}}, "vbo_num");
          }
          for(const auto& pnd : vo.panden) {
            sqw.addValue({{"vbo", id}, {"pnd", pnd}}, "vbo_pnd");
          }
        }
        else if(type=="Objecten:Nummeraanduiding") {
          if(outdated(stand))
            continue;

          NummerAanduiding na;

          string id = stand.child("Objecten:identificatie").begin()->value();
          na.huisnummer = atoi(stand.child("Objecten:huisnummer").begin()->value());
          if(auto iter = stand.child("Objecten:huisletter"); iter)
            na.huisletter = iter.begin()->value();
          if(auto iter = stand.child("Objecten:huisnummertoevoeging"); iter)
            na.toevoeging = iter.begin()->value();
          if(auto iter = stand.child("Objecten:postcode"); iter)
            na.postcode = iter.begin()->value();
          
          na.ligtAan = stand.child("Objecten:ligtAan").child("Objecten-ref:OpenbareRuimteRef").begin()->value();

          string woonplaats;
          // overrides the street Woonplaats
          if(auto iter = stand.child("Objecten:ligtIn"); iter) {
            na.ligtIn = atoi(iter.child("Objecten-ref:WoonplaatsRef").begin()->value());
            woonplaats=wpls[na.ligtIn];
          }
          else
            woonplaats=wpls[oprs[na.ligtAan].ligtIn];

          if(woonplaats.empty()) {
            cout<<na.ligtAan<<endl;
            cout<<oprs[na.ligtAan].ligtIn<<endl;
            cout<<"Failure to look up woonplaats for Nummeraanduiding id "<<id<<", make sure to parse WPL and OPR files first!"<<endl;
            exit(1);
          }
          if(auto iter = stand.child("Objecten:status"); iter)
            na.status = iter.begin()->value();


          
          if(na.ligtIn >=0)
            sqw.addValue({{"id", id}, {"ligtAanRef", na.ligtAan}, {"ligtInRef", na.ligtIn}, {"woonplaats", woonplaats}, {"postcode", na.postcode}, {"huisnummer", na.huisnummer}, {"huisletter", na.huisletter}, {"huistoevoeging", na.toevoeging}, {"status", na.status}}, "nums");
          else
            sqw.addValue({{"id", id}, {"ligtAanRef", na.ligtAan}, {"woonplaats", woonplaats}, {"postcode", na.postcode}, {"huisnummer", na.huisnummer}, {"huisletter", na.huisletter}, {"huistoevoeging", na.toevoeging}, {"status", na.status}}, "nums");

            
              //          nums[id]=na;
        }
        else if(type=="Objecten:OpenbareRuimte") {
          if(outdated(stand))
            continue;
          OpenbareRuimte opr;
          opr.naam = stand.child("Objecten:naam").begin()->value();
          opr.type = stand.child("Objecten:type").begin()->value();
          opr.ligtIn= atoi(stand.child("Objecten:ligtIn").child("Objecten-ref:WoonplaatsRef").begin()->value());
          string id = stand.child("Objecten:identificatie").begin()->value();
          if(auto iter = stand.child("Objecten:status"); iter)
            opr.status = iter.begin()->value();
          auto pnode=stand.select_nodes("Objecten:verkorteNaam/nen5825:VerkorteNaamOpenbareRuimte/nen5825:verkorteNaam");
          if(pnode.begin() != pnode.end()) {
            opr.verkorteNaam=pnode.begin()->node().begin()->value();
            sqw.addValue({{"id", id}, {"naam", opr.naam}, {"verkorteNaam", opr.verkorteNaam}, {"type", opr.type}, {"ligtInRef", opr.ligtIn}}, "oprs");
          }
          else
            sqw.addValue({{"id", id}, {"naam", opr.naam}, {"type", opr.type}, {"ligtInRef", opr.ligtIn}}, "oprs");
          oprs[id]=opr;
        }
        else
          cout<<stand.name() << '\n';
      }
    }
  }
}

