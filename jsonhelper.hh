#pragma once

#include "sqlwriter.hh"
#include "json.hpp"

// turn results from SQLiteWriter into JSON
nlohmann::json packResultsJson(const std::vector<std::unordered_map<std::string, MiniSQLite::outvar_t>>& result, bool fillnull=true);

// helper that makes a JSON string for you
std::string packResultsJsonStr(const std::vector<std::unordered_map<std::string, MiniSQLite::outvar_t>>& result, bool fillnull=true);

