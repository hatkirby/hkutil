#ifndef DATABASE_H_0B0A47D2
#define DATABASE_H_0B0A47D2

#include <sqlite3.h>
#include <fstream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <list>
#include <memory>
#include "../vendor/variant.hpp"
#include "string.h"

namespace hatkirby {

  class sqlite3_error : public std::runtime_error {
  public:

    sqlite3_error(
      const std::string& what,
      sqlite3* ppdb)
        : std::runtime_error(generateWhat(what, ppdb))
    {
    }

  private:

    static std::string generateWhat(
      const std::string& what,
      sqlite3* ppdb)
    {
      std::string errmsg(sqlite3_errmsg(ppdb));

      return what + " (" + errmsg + ")";
    }

  };

  enum class dbmode {
    read,
    readwrite,
    create
  };

  using binding =
    mpark::variant<
      std::string,
      int>;

  using column =
    std::tuple<
      std::string,
      binding>;

  class database {
  public:

    // Constructor

    explicit database(
      std::string path,
      dbmode mode)
    {
      if (mode == dbmode::create)
      {
        // If there is already a file at this path, overwrite it.
        if (std::ifstream(path))
        {
          if (std::remove(path.c_str()))
          {
            throw std::logic_error("Could not overwrite file at path");
          }
        }
      }

      sqlite3* tempDb;

      int flags;

      if (mode == dbmode::read)
      {
        flags = SQLITE_OPEN_READONLY;
      } else {
        flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
      }

      int ret = sqlite3_open_v2(
        path.c_str(),
        &tempDb,
        flags,
        NULL);

      ppdb_ = ppdb_type(tempDb);

      if (ret != SQLITE_OK)
      {
        throw sqlite3_error("Could not create output datafile", ppdb_.get());
      }
    }

    // Actions

    void execute(std::string query)
    {
      sqlite3_stmt* ppstmt;

      if (sqlite3_prepare_v2(
        ppdb_.get(),
        query.c_str(),
        query.length(),
        &ppstmt,
        NULL) != SQLITE_OK)
      {
        throw sqlite3_error("Error writing to database", ppdb_.get());
      }

      int result = sqlite3_step(ppstmt);
      sqlite3_finalize(ppstmt);

      if (result != SQLITE_DONE)
      {
        throw sqlite3_error("Error writing to database", ppdb_.get());
      }
    }

    void insertIntoTable(
      std::string table,
      std::list<column> columns)
    {
      std::list<std::string> fieldNames;
      std::list<std::string> qs;

      for (const column& c : columns)
      {
        fieldNames.push_back(std::get<0>(c));
        qs.push_back("?");
      }

      std::ostringstream query;
      query << "INSERT INTO ";
      query << table;
      query << " (";
      query << implode(std::begin(fieldNames), std::end(fieldNames), ", ");
      query << ") VALUES (";
      query << implode(std::begin(qs), std::end(qs), ", ");
      query << ")";

      std::string query_str = query.str();

      sqlite3_stmt* ppstmt;

      if (sqlite3_prepare_v2(
        ppdb_.get(),
        query_str.c_str(),
        query_str.length(),
        &ppstmt,
        NULL) != SQLITE_OK)
      {
        throw sqlite3_error("Error writing to database", ppdb_.get());
      }

      int i = 1;
      for (const column& c : columns)
      {
        const binding& b = std::get<1>(c);

        if (mpark::holds_alternative<int>(b))
        {
          sqlite3_bind_int(ppstmt, i, mpark::get<int>(b));
        } else if (mpark::holds_alternative<std::string>(b))
        {
          const std::string& arg = mpark::get<std::string>(b);

          sqlite3_bind_text(
            ppstmt,
            i,
            arg.c_str(),
            arg.length(),
            SQLITE_TRANSIENT);
        }

        i++;
      }

      int result = sqlite3_step(ppstmt);
      sqlite3_finalize(ppstmt);

      if (result != SQLITE_DONE)
      {
        throw sqlite3_error("Error writing to database", ppdb_.get());
      }
    }

  private:

    class sqlite3_deleter {
    public:

      void operator()(sqlite3* ptr) const
      {
        sqlite3_close_v2(ptr);
      }
    };

    using ppdb_type = std::unique_ptr<sqlite3, sqlite3_deleter>;

    ppdb_type ppdb_;

  };

};

#endif /* end of include guard: DATABASE_H_0B0A47D2 */
