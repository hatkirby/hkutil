#ifndef DATABASE_H_0B0A47D2
#define DATABASE_H_0B0A47D2

#include <sqlite3.h>
#include <fstream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <vector>
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

  using blob_type = std::vector<unsigned char>;

  using binding =
    mpark::variant<
      std::string,
      int,
      double,
      std::nullptr_t,
      blob_type>;

  using column =
    std::tuple<
      std::string,
      binding>;

  using row =
    std::vector<binding>;

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
      sqlite3_stmt* tempStmt;

      int ret = sqlite3_prepare_v2(
        ppdb_.get(),
        query.c_str(),
        query.length(),
        &tempStmt,
        NULL);

      ppstmt_type ppstmt(tempStmt);

      if (ret != SQLITE_OK)
      {
        throw sqlite3_error("Error writing to database", ppdb_.get());
      }

      if (sqlite3_step(ppstmt.get()) != SQLITE_DONE)
      {
        throw sqlite3_error("Error writing to database", ppdb_.get());
      }
    }

    uint64_t insertIntoTable(
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

      sqlite3_stmt* tempStmt;

      int ret = sqlite3_prepare_v2(
        ppdb_.get(),
        query_str.c_str(),
        query_str.length(),
        &tempStmt,
        NULL);

      ppstmt_type ppstmt(tempStmt);

      if (ret != SQLITE_OK)
      {
        throw sqlite3_error("Error writing to database", ppdb_.get());
      }

      int i = 1;
      for (const column& c : columns)
      {
        bindToStmt(ppstmt, i, std::get<1>(c));

        i++;
      }

      if (sqlite3_step(ppstmt.get()) != SQLITE_DONE)
      {
        throw sqlite3_error("Error writing to database", ppdb_.get());
      }

      return sqlite3_last_insert_rowid(ppdb_.get());
    }

    std::vector<row> queryAll(
      std::string queryString,
      std::list<binding> bindings = {})
    {
      sqlite3_stmt* tempStmt;

      int ret = sqlite3_prepare_v2(
        ppdb_.get(),
        queryString.c_str(),
        queryString.length(),
        &tempStmt,
        NULL);

      ppstmt_type ppstmt(tempStmt);

      if (ret != SQLITE_OK)
      {
        throw sqlite3_error("Error preparing query", ppdb_.get());
      }

      int i = 1;
      for (const binding& value : bindings)
      {
        bindToStmt(ppstmt, i, value);

        i++;
      }

      std::vector<row> result;

      while (sqlite3_step(ppstmt.get()) == SQLITE_ROW)
      {
        row curRow;

        int cols = sqlite3_column_count(ppstmt.get());

        for (int i = 0; i < cols; i++)
        {
          switch (sqlite3_column_type(ppstmt.get(), i))
          {
            case SQLITE_INTEGER:
            {
              curRow.emplace_back(sqlite3_column_int(ppstmt.get(), i));

              break;
            }

            case SQLITE_TEXT:
            {
              curRow.emplace_back(
                std::string(
                  reinterpret_cast<const char*>(
                    sqlite3_column_text(ppstmt.get(), i))));

              break;
            }

            case SQLITE_FLOAT:
            {
              curRow.emplace_back(sqlite3_column_double(ppstmt.get(), i));

              break;
            }

            case SQLITE_NULL:
            {
              curRow.emplace_back(nullptr);

              break;
            }

            case SQLITE_BLOB:
            {
              int len = sqlite3_column_bytes(ppstmt.get(), i);

              blob_type value(len);

              memcpy(
                value.data(),
                sqlite3_column_blob(ppstmt.get(), i),
                len);

                curRow.emplace_back(std::move(value));

              break;
            }

            default:
            {
              // Impossible

              break;
            }
          }
        }

        result.emplace_back(std::move(curRow));
      }

      return result;
    }

    row queryFirst(
      std::string queryString,
      std::list<binding> bindings = {})
    {
      std::vector<row> dataset = queryAll(
        std::move(queryString),
        std::move(bindings));

      if (dataset.empty())
      {
        throw std::logic_error("Query returned empty dataset");
      }

      row result = std::move(dataset.front());

      return result;
    }

  private:

    class sqlite3_deleter {
    public:

      void operator()(sqlite3* ptr) const
      {
        sqlite3_close_v2(ptr);
      }
    };

    class stmt_deleter {
    public:

      void operator()(sqlite3_stmt* ptr) const
      {
        sqlite3_finalize(ptr);
      }
    };

    using ppdb_type = std::unique_ptr<sqlite3, sqlite3_deleter>;
    using ppstmt_type = std::unique_ptr<sqlite3_stmt, stmt_deleter>;

    void bindToStmt(
      const ppstmt_type& ppstmt,
      int i,
      const binding& value)
    {
      if (mpark::holds_alternative<int>(value))
      {
        if (sqlite3_bind_int(
          ppstmt.get(),
          i,
          mpark::get<int>(value)) != SQLITE_OK)
        {
          throw sqlite3_error("Error preparing statement", ppdb_.get());
        }
      } else if (mpark::holds_alternative<std::string>(value))
      {
        const std::string& arg = mpark::get<std::string>(value);

        if (sqlite3_bind_text(
          ppstmt.get(),
          i,
          arg.c_str(),
          arg.length(),
          SQLITE_TRANSIENT) != SQLITE_OK)
        {
          throw sqlite3_error("Error preparing statement", ppdb_.get());
        }
      } else if (mpark::holds_alternative<double>(value))
      {
        if (sqlite3_bind_double(
          ppstmt.get(),
          i,
          mpark::get<double>(value)) != SQLITE_OK)
        {
          throw sqlite3_error("Error preparing statement", ppdb_.get());
        }
      } else if (mpark::holds_alternative<std::nullptr_t>(value))
      {
        if (sqlite3_bind_null(ppstmt.get(), i) != SQLITE_OK)
        {
          throw sqlite3_error("Error preparing statement", ppdb_.get());
        }
      } else if (mpark::holds_alternative<blob_type>(value))
      {
        const blob_type& arg = mpark::get<blob_type>(value);

        if (sqlite3_bind_blob(
          ppstmt.get(),
          i,
          arg.data(),
          arg.size(),
          SQLITE_TRANSIENT) != SQLITE_OK)
        {
          throw sqlite3_error("Error preparing statement", ppdb_.get());
        }
      }
    }

    ppdb_type ppdb_;

  };

};

#endif /* end of include guard: DATABASE_H_0B0A47D2 */
