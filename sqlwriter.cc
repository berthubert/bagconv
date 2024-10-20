#include "sqlwriter.hh"
#include <algorithm>
#include <unistd.h>
#include "sqlite3.h"
using namespace std;

MiniSQLite::MiniSQLite(std::string_view fname, SQLWFlag flag)
{
  int flags;
  if(flag == SQLWFlag::ReadOnly)
    flags = SQLITE_OPEN_READONLY;
  else
    flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
  
  if ( sqlite3_open_v2(&fname[0], &d_sqlite, flags, 0)!=SQLITE_OK) {
    throw runtime_error("Unable to open "+(string)fname+" for sqlite");
  }
  sqlite3_extended_result_codes(d_sqlite, 1);
  exec("PRAGMA journal_mode='wal'");
  exec("PRAGMA foreign_keys=ON");
  sqlite3_busy_timeout(d_sqlite, 60000);
}

//! Get field names and types from a table
vector<pair<string,string> > MiniSQLite::getSchema(const std::string& table)
{
  vector<pair<string,string>> ret;
  
  auto rows = exec("SELECT cid,name,type FROM pragma_table_xinfo('"+table+"')");

  for(const auto& r : rows) {
    ret.push_back({r[1], r[2]});
  }
  sort(ret.begin(), ret.end(), [](const auto& a, const auto& b) {
    return a.first < b.first;
  });

  //  cout<<"returning "<<ret.size()<<" rows for table "<<table<<"\n";
  return ret;
}

int MiniSQLite::helperFunc(void* ptr, int cols, char** colvals, char** colnames [[maybe_unused]])
{
  vector<string> row;
  row.reserve(cols);
  for(int n=0; n < cols ; ++n)
    row.push_back(colvals[n]);
  ((MiniSQLite*)ptr)->d_rows.push_back(row);
  return 0;
}

vector<vector<string>> MiniSQLite::exec(std::string_view str)
{
  char *errmsg;
  std::string errstr;
  //  int (*callback)(void*,int,char**,char**)
  d_rows.clear();
  int rc = sqlite3_exec(d_sqlite, &str[0], helperFunc, this, &errmsg);
  if (rc != SQLITE_OK) {
    errstr = errmsg;
    sqlite3_free(errmsg);
    throw std::runtime_error("Error executing sqlite3 query '"+(string)str+"': "+errstr);
  }
  return d_rows; 
}

static void checkBind(int rc)
{
  if(rc) {
    throw std::runtime_error("Error binding value to prepared statement: " + string(sqlite3_errstr(rc)));
  }
}

void MiniSQLite::bindPrep(const std::string& table, int idx, bool value) {   checkBind(sqlite3_bind_int(d_stmts[table], idx, value ? 1 : 0));   }
void MiniSQLite::bindPrep(const std::string& table, int idx, int value) {   checkBind(sqlite3_bind_int(d_stmts[table], idx, value));   }
void MiniSQLite::bindPrep(const std::string& table, int idx, uint32_t value) {   checkBind(sqlite3_bind_int64(d_stmts[table], idx, value));   }
void MiniSQLite::bindPrep(const std::string& table, int idx, long value) {   checkBind(sqlite3_bind_int64(d_stmts[table], idx, value));   }
void MiniSQLite::bindPrep(const std::string& table, int idx, unsigned long value) {   checkBind(sqlite3_bind_int64(d_stmts[table], idx, value));   }
void MiniSQLite::bindPrep(const std::string& table, int idx, long long value) {   checkBind(sqlite3_bind_int64(d_stmts[table], idx, value));   }
void MiniSQLite::bindPrep(const std::string& table, int idx, unsigned long long value) {   checkBind(sqlite3_bind_int64(d_stmts[table], idx, value));   }
void MiniSQLite::bindPrep(const std::string& table, int idx, double value) {   checkBind(sqlite3_bind_double(d_stmts[table], idx, value));   }
void MiniSQLite::bindPrep(const std::string& table, int idx, const std::string& value) {   checkBind(sqlite3_bind_text(d_stmts[table], idx, value.c_str(), value.size(), SQLITE_TRANSIENT));   }
void MiniSQLite::bindPrep(const std::string& table, int idx, const std::vector<uint8_t>& value) {
  if(value.empty())
    checkBind(sqlite3_bind_zeroblob(d_stmts[table], idx, 0));
  else
    checkBind(sqlite3_bind_blob(d_stmts[table], idx, &value.at(0), value.size(), SQLITE_TRANSIENT));
}


void MiniSQLite::prepare(const std::string& table, string_view str)
{
  if(d_stmts[table]) {
    sqlite3_finalize(d_stmts[table]);
    d_stmts[table] = 0;
  }
  const char* pTail;
  
  if (sqlite3_prepare_v2(d_sqlite, &str[0], -1, &d_stmts[table], &pTail ) != SQLITE_OK) {
    throw runtime_error("Unable to prepare query "+(string)str + ": "+sqlite3_errmsg(d_sqlite));
  }
}

struct DeadlineCatcher
{
  DeadlineCatcher(sqlite3* db, unsigned int msec) : d_sqlite(db), d_msec(msec)
  {
    if(!msec)
      return;
    clock_gettime(CLOCK_MONOTONIC, &d_ttd);
    auto r = div(msec, 1000); // seconds
    d_ttd.tv_sec += r.quot;
    d_ttd.tv_nsec += 1000000 * r.rem;
    
    if(d_ttd.tv_nsec > 1000000000) {
      d_ttd.tv_sec++;
      d_ttd.tv_nsec -= 1000000000;
    }
    sqlite3_progress_handler(d_sqlite, 100, [](void *ptr) -> int {
      DeadlineCatcher* us = (DeadlineCatcher*) ptr;
      us->called++;
      struct timespec now;
      clock_gettime(CLOCK_MONOTONIC, &now);
      return std::tie(us->d_ttd.tv_sec, us->d_ttd.tv_nsec) <
	std::tie(now.tv_sec, now.tv_nsec);
    }, this);
  }

  DeadlineCatcher(const DeadlineCatcher& rhs) = delete;
  
  ~DeadlineCatcher()
  {
    if(d_msec) {
      sqlite3_progress_handler(d_sqlite, 0, 0, 0); // remove
      //      cout<<"Was called "<<called<<" times\n";
    }
  }
  sqlite3* d_sqlite;
  const unsigned int d_msec;
  struct timespec d_ttd;
  unsigned int called = 0;
};

void MiniSQLite::execPrep(const std::string& table, std::vector<std::unordered_map<std::string, outvar_t>>* rows, unsigned int msec)
{
  int rc;
  if(rows)
    rows->clear();

  DeadlineCatcher dc(d_sqlite, msec); // noop if msec = 0
  
  std::unordered_map<string, outvar_t> row;
  for(;;) {
    rc = sqlite3_step(d_stmts[table]); 
    if(rc == SQLITE_DONE)
      break;
    else if(rows && rc == SQLITE_ROW) {
      row.clear();
      for(int n = 0 ; n < sqlite3_column_count(d_stmts[table]);++n) {
        int type = sqlite3_column_type(d_stmts[table], n);
        
        if(type == SQLITE_TEXT) {
          const char* p = (const char*)sqlite3_column_text(d_stmts[table], n);
          if(!p) {
            row[sqlite3_column_name(d_stmts[table], n)] = string();
          }
          else
            row[sqlite3_column_name(d_stmts[table], n)] = string(p, sqlite3_column_bytes(d_stmts[table], n));
        }
        else if(type == SQLITE_BLOB) {
          const uint8_t* p = (const uint8_t*)sqlite3_column_blob(d_stmts[table], n);
          if(!p) {
            row[sqlite3_column_name(d_stmts[table], n)]= vector<uint8_t>();
          }
          else
            row[sqlite3_column_name(d_stmts[table], n)]=vector<uint8_t>(p, p+sqlite3_column_bytes(d_stmts[table], n));
        }
        else if(type == SQLITE_FLOAT) {
          row[sqlite3_column_name(d_stmts[table], n)]= sqlite3_column_double(d_stmts[table], n);
        }
        else if(type == SQLITE_INTEGER) {
          row[sqlite3_column_name(d_stmts[table], n)]= sqlite3_column_int64(d_stmts[table], n);
        }
        else if(type == SQLITE_NULL) {
          row[sqlite3_column_name(d_stmts[table], n)]= nullptr;
        }
        
      }
      rows->push_back(row);
    }
    else {
      sqlite3_reset(d_stmts[table]);
      sqlite3_clear_bindings(d_stmts[table]);
      throw runtime_error("Sqlite error "+std::to_string(rc)+": "+sqlite3_errstr(rc));
    }
  }
  rc= sqlite3_reset(d_stmts[table]);
  if(rc != SQLITE_OK)
    throw runtime_error("Sqlite error "+std::to_string(rc)+": "+sqlite3_errstr(rc));
  sqlite3_clear_bindings(d_stmts[table]);
}

void MiniSQLite::begin()
{
  d_intransaction=true;
  exec("begin");
}
void MiniSQLite::commit()
{
  d_intransaction=false;
  exec("commit");
}

void MiniSQLite::cycle()
{
  exec("commit;begin");
}

bool MiniSQLite::haveTable(const string& table)
{
  return !getSchema(table).empty();
}


//! Add a column to a table with a certain type
void MiniSQLite::addColumn(const string& table, string_view name, string_view type, const std::string& meta)
{
  // SECURITY PROBLEM - somehow we can't do prepared statements here
  
  if(!haveTable(table)) {
#if SQLITE_VERSION_NUMBER >= 3037001
    exec("create table if not exists '"+table+"' ( '"+(string)name+"' "+(string)type+" "+meta+") STRICT");
#else
    exec("create table if not exists '"+table+"' ( '"+(string)name+"' "+(string)type+" "+ meta+")");
#endif
  } else {
    //    cout<<"Adding column "<<name<<" to table "<<table<<endl;
    exec("ALTER table \""+table+"\" add column \""+string(name)+ "\" "+string(type)+ " "+meta);
  }
}



void SQLiteWriter::commitThread()
{
  int n=0;
  while(!d_pleasequit) {
    usleep(50000);
    if(!(n%20)) {
      std::lock_guard<std::mutex> lock(d_mutex);
      d_db.cycle();
    }
    n++;
  }
  //  cerr<<"Thread exiting"<<endl;
}

bool SQLiteWriter::haveColumn(const std::string& table, std::string_view name)
{
  if(d_columns[table].empty()) {
    d_columns[table] = d_db.getSchema(table);
  }
  //  cout<<"Do we have column "<<name<<" in table "<<table<<endl;
  // this could be more efficient somehow
  pair<string, string> cmp{name, std::string()};
  return binary_search(d_columns[table].begin(), d_columns[table].end(), cmp,
                       [](const auto& a, const auto& b)
                       {
                         return a.first < b.first;
                       });

}


void SQLiteWriter::addValue(const initializer_list<std::pair<const char*, var_t>>& values, const std::string& table)
{
  addValueGeneric(table, values);
}

void SQLiteWriter::addValue(const std::vector<std::pair<const char*, var_t>>& values, const std::string& table)
{
  addValueGeneric(table, values);
}

void SQLiteWriter::addOrReplaceValue(const initializer_list<std::pair<const char*, var_t>>& values, const std::string& table)
{
  addValueGeneric(table, values, true);
}

void SQLiteWriter::addOrReplaceValue(const std::vector<std::pair<const char*, var_t>>& values, const std::string& table)
{
  addValueGeneric(table, values, true);
}



template<typename T>
void SQLiteWriter::addValueGeneric(const std::string& table, const T& values, bool replace)
{
  if(d_flag == SQLWFlag::ReadOnly)
    throw std::runtime_error("Attempting to write to a read-only database instance");
  
  std::lock_guard<std::mutex> lock(d_mutex);
  if(!d_db.isPrepared(table) || d_lastreplace[table] != replace || !equal(values.begin(), values.end(),
                                       d_lastsig[table].cbegin(), d_lastsig[table].cend(),
                            [](const auto& a, const auto& b)
  {
    return a.first == b;
  })) {
    //    cout<<"Starting a new prepared statement"<<endl;
    string q = string("insert ") + (replace ? "or replace " : "") + "into '"+ table+"' (";
    string qmarks;
    bool first=true;
    for(const auto& p : values) {
      if(!haveColumn(table, p.first)) {
        if(std::get_if<double>(&p.second)) {
          d_db.addColumn(table, p.first, "REAL", d_meta[table][p.first]);
          d_columns[table].push_back({p.first, "REAL"});
        }
        else if(std::get_if<string>(&p.second)) {
          d_db.addColumn(table, p.first, "TEXT", d_meta[table][p.first]);
          d_columns[table].push_back({p.first, "TEXT"});
        }
        else if(std::get_if<vector<uint8_t>>(&p.second)) {
          d_db.addColumn(table, p.first, "BLOB", d_meta[table][p.first]);
          d_columns[table].push_back({p.first, "BLOB"});
        }
        else  {
          d_db.addColumn(table, p.first, "INT", d_meta[table][p.first]);
          d_columns[table].push_back({p.first, "INT"});
        }

        sort(d_columns[table].begin(), d_columns[table].end());
      }
      if(!first) {
        q+=", ";
        qmarks += ", ";
      }
      first=false;
      q+="'"+string(p.first)+"'";
      qmarks +="?";
    }
    q+= ") values ("+qmarks+")";

    d_db.prepare(table, q);
    
    d_lastsig[table].clear();
    for(const auto& p : values)
      d_lastsig[table].push_back(p.first);
    d_lastreplace[table]=replace;
  }
  
  int n = 1;
  for(const auto& p : values) {
    std::visit([this, &n, &table](auto&& arg) {
      d_db.bindPrep(table, n, arg);
    }, p.second);
    n++;
  }
  d_db.execPrep(table);
}

std::vector<std::unordered_map<std::string, std::string>> SQLiteWriter::query(const std::string& q, const initializer_list<var_t>& values)
{
  auto res = queryGen(q, values);
  std::vector<std::unordered_map<std::string, std::string>> ret;
  for(const auto& rowin : res) {
    std::unordered_map<std::string, std::string> rowout;
    for(const auto& f : rowin) {
      string str;
      std::visit([&str](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, string>)
                       str=arg;
        else if constexpr (std::is_same_v<T, nullptr_t>)
                       str="";
        else if constexpr (std::is_same_v<T, vector<uint8_t>>)
          str = "<blob>";
        else str = to_string(arg);
      }, f.second);
      rowout[f.first] = str;
    }
    ret.push_back(rowout);
  }
  return ret;
}

std::vector<std::unordered_map<std::string, MiniSQLite::outvar_t>> SQLiteWriter::queryT(const std::string& q, const initializer_list<var_t>& values, unsigned int msec)
{
  return queryGen(q, values, msec);
}

template<typename T>
vector<std::unordered_map<string, MiniSQLite::outvar_t>> SQLiteWriter::queryGen(const std::string& q, const T& values, unsigned int msec)
{
  if(msec && d_flag != SQLWFlag::ReadOnly)
    throw std::runtime_error("Timeout only possible for read-only connections");
  
  std::lock_guard<std::mutex> lock(d_mutex);
  d_db.prepare("", q); // we use an empty table name so as not to collide with other things
  int n = 1;
  for(const auto& p : values) {
    std::visit([this, &n](auto&& arg) {
      d_db.bindPrep("", n, arg);
    }, p);
    n++;
  }
  vector<unordered_map<string, MiniSQLite::outvar_t>> ret;
  d_db.execPrep("", &ret, msec);

  return ret;
}


MiniSQLite::~MiniSQLite()
{
  // needs to close down d_sqlite3
  if(d_intransaction)
    commit();

  for(auto& stmt: d_stmts)
    if(stmt.second)
      sqlite3_finalize(stmt.second);
  
  sqlite3_close(d_sqlite); // same
}
