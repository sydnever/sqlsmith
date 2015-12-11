#include "schema.hh"
#include "relmodel.hh"
#include <pqxx/pqxx>
#include "gitrev.h"

using namespace std;
using namespace pqxx;

schema_pqxx::schema_pqxx(std::string &conninfo) {
  connection c(conninfo);
  work w(c);

  w.exec("set application_name to 'sqlsmith " GITREV "';");
    
  result r = w.exec("select version()");
  version = r[0][0].as<string>();
  
  cerr << "Loading tables...";
  r = w.exec("select table_name, "
		    "table_schema, "
	            "is_insertable_into, "
	            "table_type "
	     "from information_schema.tables;");

  for (auto row = r.begin(); row != r.end(); ++row) {
    string schema(row[1].as<string>());
    string insertable(row[2].as<string>());
    string table_type(row[3].as<string>());
    //       if (schema == "pg_catalog")
    // 	continue;
    //       if (schema == "information_schema")
    // 	continue;
      
    tables.push_back(table(row[0].as<string>(),
			   schema,
			   ((insertable == "YES") ? true : false),
			   ((table_type == "BASE TABLE") ? true : false)));
  }
  cerr << "done." << endl;

  cerr << "Loading columns...";

  for (auto t = tables.begin(); t != tables.end(); ++t) {
    string q("select column_name, "
	     "udt_name"
	     " from information_schema.columns where"
	     " table_catalog = current_catalog");
    q += " and table_schema = " + w.quote(t->schema);
    q += " and table_name = " + w.quote(t->name);
    q += ";";

    r = w.exec(q);
    for (auto row : r) {
      column c(row[0].as<string>(), row[1].as<string>());
      t->columns().push_back(c);
    }
  }
  cerr << "done." << endl;

  cerr << "Loading operators...";

  r = w.exec("select oprname, oprleft::regtype,"
		    "oprright::regtype, oprresult::regtype "
		    "from pg_catalog.pg_operator;");
  for (auto row : r) {
    op o(row[0].as<string>(),
	 row[1].as<string>(),
	 row[2].as<string>(),
	 row[3].as<string>());
    register_operator(o);
  }

  cerr << "done." << endl;

  booltype = sqltype::get("boolean");
  inttype = sqltype::get("integer");

  cerr << "Loading routines...";
  r = w.exec("select specific_schema, specific_name, data_type, routine_name "
	     "from information_schema.routines "
	     "where specific_catalog = current_catalog");

  for (auto row : r) {
    routine proc(row[0].as<string>(),
		 row[1].as<string>(),
		 sqltype::get(row[2].as<string>()),
		 row[3].as<string>());
    routines.push_back(proc);
  }

  cerr << "done." << endl;

  cerr << "Loading parameters...";

  for (auto &proc : routines) {
    string q("select data_type "
	     "from information_schema.parameters "
	     "where specific_catalog = current_catalog ");
    q += " and specific_name = " + w.quote(proc.specific_name);
    q += " and specific_schema = " + w.quote(proc.schema);
    q += " order by ordinal_position asc";
      
    r = w.exec(q);
    for (auto row : r) {
      proc.argtypes.push_back(sqltype::get(row[0].as<string>()));
    }
  }
  cerr << "done." << endl;

}
