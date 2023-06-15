#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <mutex>
#include <thread>
#include <iostream>

struct sqlite3;
struct sqlite3_stmt;

class MiniSQLite
{
public:
  MiniSQLite(std::string_view fname);
  ~MiniSQLite();
  std::vector<std::pair<std::string, std::string>> getSchema(const std::string& table);
  void addColumn(const std::string& table, std::string_view name, std::string_view type);
  std::vector<std::vector<std::string>> exec(std::string_view query);
  void prepare(const std::string& table, std::string_view str);
  void bindPrep(const std::string& table, int idx, bool value);
  void bindPrep(const std::string& table, int idx, int value);
  void bindPrep(const std::string& table, int idx, uint32_t value);
  void bindPrep(const std::string& table, int idx, long value);
  void bindPrep(const std::string& table, int idx, unsigned long value);
  void bindPrep(const std::string& table, int idx, long long value); 
  void bindPrep(const std::string& table, int idx, unsigned long long value);
  void bindPrep(const std::string& table, int idx, double value);
  void bindPrep(const std::string& table, int idx, const std::string& value);
  void execPrep(const std::string& table); 
  void begin();
  void commit();
  void cycle();
  bool isPrepared(const std::string& table) const
  {
    if(auto iter = d_stmts.find(table); iter == d_stmts.end())
      return false;
    else
      return iter->second != nullptr;
  }

private:
  sqlite3* d_sqlite;
  std::unordered_map<std::string, sqlite3_stmt*> d_stmts;
  std::vector<std::vector<std::string>> d_rows; // for exec()
  static int helperFunc(void* ptr, int cols, char** colvals, char** colnames);
  bool d_intransaction{false};
  bool haveTable(const std::string& table);
};

class SQLiteWriter
{

public:
  explicit SQLiteWriter(std::string_view fname) : d_db(fname)
  {
    //    for(const auto& c : d_columns)
    //      cout <<c.first<<"\t"<<c.second<<endl;

    d_db.exec("PRAGMA journal_mode='wal'");
    d_db.begin(); // open the transaction
    d_thread = std::thread(&SQLiteWriter::commitThread, this);
  }
  typedef std::variant<double, int32_t, uint32_t, int64_t, std::string> var_t;
  void addValue(const std::initializer_list<std::pair<const char*, var_t>>& values, const std::string& table="data");
  void addValue(const std::vector<std::pair<const char*, var_t>>& values, const std::string& table="data");
  
  template<typename T>
  void addValueGeneric(const std::string& table, const T& values);
  ~SQLiteWriter()
  {
    //    std::cerr<<"Destructor called"<<std::endl;
    d_pleasequit=true;
    d_thread.join();
  }

private:
  void commitThread();
  bool d_pleasequit{false};
  std::thread d_thread;
  std::mutex d_mutex;  
  MiniSQLite d_db;
  std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> d_columns;
  std::unordered_map<std::string, std::vector<std::string>> d_lastsig;
  bool haveColumn(const std::string& table, std::string_view name);

};
