#include "sqlwriter.hh"
#include "json.hpp"

using namespace std;

nlohmann::json packResultsJson(const vector<unordered_map<string, MiniSQLite::outvar_t>>& result, bool fillnull)
{
  nlohmann::json arr = nlohmann::json::array();
  
  for(const auto& row : result) {
    nlohmann::json j;
    for(auto& col : row) {
      std::visit([&j, &col, &fillnull](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, nullptr_t>) {
          if(fillnull)
	    j[col.first]="";
	  else
	    return;
        }
        else  {
          j[col.first] = arg;
        }
      }, col.second);
    }
    arr += j;
  }
  return arr;
}

string packResultsJsonStr(const vector<unordered_map<string, MiniSQLite::outvar_t>>& result, bool fillnull)
{
  return packResultsJson(result, fillnull).dump();
}
