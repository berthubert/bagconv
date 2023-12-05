#include "sqlwriter.hh"
#include "nlohmann/json.hpp"

using namespace std;


nlohmann::json packResultsJson(const vector<unordered_map<string, MiniSQLite::outvar_t>>& result)
{
  nlohmann::json arr = nlohmann::json::array();
  
  for(const auto& row : result) {
    nlohmann::json j;
    for(auto& col : row) {
      std::visit([&j, &col](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, nullptr_t>)
                       return;
        else  {
          j[col.first] = arg;
        }

      }, col.second);
    }
    arr += j;
  }
  return arr;
}

string packResultsJsonStr(const vector<unordered_map<string, MiniSQLite::outvar_t>>& result)
{
  return packResultsJson(result).dump();
}
