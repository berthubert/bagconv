#include "sqlwriter.hh"
#include <algorithm>
#include <unistd.h>
#include "sqlite3.h"
using namespace std;

MiniSQLite::MiniSQLite(std::string_view fname)
{
  if ( sqlite3_open(&fname[0], &d_sqlite)!=SQLITE_OK ) {
    throw runtime_error("Unable to open "+(string)fname+" for sqlite");
  }
  exec("PRAGMA journal_mode='wal'");
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

void MiniSQLite::bindPrep(const std::string& table, int idx, bool value) {   sqlite3_bind_int(d_stmts[table], idx, value ? 1 : 0);   }
void MiniSQLite::bindPrep(const std::string& table, int idx, int value) {   sqlite3_bind_int(d_stmts[table], idx, value);   }
void MiniSQLite::bindPrep(const std::string& table, int idx, uint32_t value) {   sqlite3_bind_int64(d_stmts[table], idx, value);   }
void MiniSQLite::bindPrep(const std::string& table, int idx, long value) {   sqlite3_bind_int64(d_stmts[table], idx, value);   }
void MiniSQLite::bindPrep(const std::string& table, int idx, unsigned long value) {   sqlite3_bind_int64(d_stmts[table], idx, value);   }
void MiniSQLite::bindPrep(const std::string& table, int idx, long long value) {   sqlite3_bind_int64(d_stmts[table], idx, value);   }
void MiniSQLite::bindPrep(const std::string& table, int idx, unsigned long long value) {   sqlite3_bind_int64(d_stmts[table], idx, value);   }
void MiniSQLite::bindPrep(const std::string& table, int idx, double value) {   sqlite3_bind_double(d_stmts[table], idx, value);   }
void MiniSQLite::bindPrep(const std::string& table, int idx, const std::string& value) {   sqlite3_bind_text(d_stmts[table], idx, value.c_str(), value.size(), SQLITE_TRANSIENT);   }


void MiniSQLite::prepare(const std::string& table, string_view str)
{
  if(d_stmts[table]) {
    sqlite3_finalize(d_stmts[table]);
    d_stmts[table] = 0;
  }
  const char* pTail;
  
  if (sqlite3_prepare_v2(d_sqlite, &str[0], -1, &d_stmts[table], &pTail ) != SQLITE_OK) {
    throw runtime_error("Unable to prepare query "+(string)str);
  }
}

void MiniSQLite::execPrep(const std::string& table)
{
  int rc;
  for(;;) {
    rc = sqlite3_step(d_stmts[table]); // XXX this needs to be an error checking loop
    if(rc == SQLITE_DONE)
      break;
    else
      throw runtime_error("Sqlite error: "+std::to_string(rc));
  }
  rc= sqlite3_reset(d_stmts[table]);
  if(rc != SQLITE_OK)
    throw runtime_error("Sqlite error: "+std::to_string(rc));
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
void MiniSQLite::addColumn(const string& table, string_view name, string_view type)
{
  // SECURITY PROBLEM - somehow we can't do prepared statements here
  
  if(!haveTable(table)) {
#if SQLITE_VERSION_NUMBER >= 3037001
    exec("create table if not exists '"+table+"' ( '"+(string)name+"' "+(string)type+" ) STRICT");
#else
    exec("create table if not exists '"+table+"' ( '"+(string)name+"' "+(string)type+" )");
#endif
  } else {
    //    cout<<"Adding column "<<name<<" to table "<<table<<endl;
    exec("ALTER table \""+table+"\" add column \""+string(name)+ "\" "+string(type));
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


template<typename T>
void SQLiteWriter::addValueGeneric(const std::string& table, const T& values)
{
  std::lock_guard<std::mutex> lock(d_mutex);
  if(!d_db.isPrepared(table) || !equal(values.begin(), values.end(),
                                       d_lastsig[table].cbegin(), d_lastsig[table].cend(),
                            [](const auto& a, const auto& b)
  {
    return a.first == b;
  })) {
    //    cout<<"Starting a new prepared statement"<<endl;
    string q = "insert into '"+table+"' (";
    string qmarks;
    bool first=true;
    for(const auto& p : values) {
      if(!haveColumn(table, p.first)) {
        if(std::get_if<double>(&p.second)) {
          d_db.addColumn(table, p.first, "REAL");
          d_columns[table].push_back({p.first, "REAL"});
        }
        else if(std::get_if<string>(&p.second)) {
          d_db.addColumn(table, p.first, "TEXT");
          d_columns[table].push_back({p.first, "TEXT"});
        } else  {
          d_db.addColumn(table, p.first, "INT");
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
