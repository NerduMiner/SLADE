
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2020 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         http://slade.mancubus.net
// Filename:    Database.cpp
// Description: Functions for working with the SLADE program database.
//              The Context class keeps connections open to a database, since
//              opening a new connection is expensive. It can also keep cached
//              sql queries (for frequent reuse)
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301, USA.
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
//
// Includes
//
// -----------------------------------------------------------------------------
#include "Main.h"
#include "Database.h"
#include "App.h"
#include "Archive/Archive.h"
#include "General/Console.h"
#include "Utility/FileUtils.h"
#include "Utility/StringUtils.h"

using namespace slade;


// -----------------------------------------------------------------------------
//
// Variables
//
// -----------------------------------------------------------------------------
namespace slade::database
{
Context            db_global;
string             template_db_path;
vector<Named<int>> table_versions = { { "archive_file", 1 } };
} // namespace slade::database


// -----------------------------------------------------------------------------
//
// Database::Context Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Context class constructor
// -----------------------------------------------------------------------------
database::Context::Context(string_view file_path)
{
	if (!file_path.empty())
		open(file_path);
}

// -----------------------------------------------------------------------------
// Opens connections to the database file at [file_path].
// Returns false if any existing connections couldn't be closed, true otherwise
// -----------------------------------------------------------------------------
bool database::Context::open(string_view file_path)
{
	if (!close())
		return false;

	file_path_     = file_path;
	connection_ro_ = std::make_unique<SQLite::Database>(file_path_, SQLite::OPEN_READONLY);
	connection_rw_ = std::make_unique<SQLite::Database>(file_path_, SQLite::OPEN_READWRITE);

	return true;
}

// -----------------------------------------------------------------------------
// Closes the context's connections to its database
// -----------------------------------------------------------------------------
bool database::Context::close()
{
	if (!connection_ro_)
		return true;

	try
	{
		cached_queries_.clear();
		connection_ro_ = nullptr;
		connection_rw_ = nullptr;
	}
	catch (std::exception& ex)
	{
		log::error("Error closing connections for database {}: {}", file_path_, ex.what());
		return false;
	}

	file_path_.clear();
	return true;
}

// -----------------------------------------------------------------------------
// Returns the cached query [id] or nullptr if not found
// -----------------------------------------------------------------------------
SQLite::Statement* database::Context::cachedQuery(string_view id)
{
	auto i = cached_queries_.find(id);
	if (i != cached_queries_.end())
	{
		i->second->tryReset();
		return i->second.get();
	}
	
	return nullptr;
}

// -----------------------------------------------------------------------------
// Returns the cached query [id] if it exists, otherwise creates a new cached
// query from the given [sql] string and returns it.
// If [writes] is true, the created query will use the read+write connection.
// -----------------------------------------------------------------------------
SQLite::Statement* database::Context::cacheQuery(string_view id, const char* sql, bool writes)
{
	// Check for existing cached query [id]
	auto i = cached_queries_.find(id);
	if (i != cached_queries_.end())
	{
		i->second->tryReset();
		return i->second.get();
	}

	// Check connection
	if (!connection_ro_)
		return nullptr;

	// Create & add cached query
	auto& db        = writes ? *connection_rw_ : *connection_ro_;
	auto  statement = std::make_unique<SQLite::Statement>(db, sql);
	auto  ptr       = statement.get();
	cached_queries_.emplace(id, std::move(statement));

	return ptr;
}

// -----------------------------------------------------------------------------
// Executes an sql [query] on the database.
// Returns the number of rows modified/created by the query, or 0 if the context
// is not connected
// -----------------------------------------------------------------------------
int database::Context::exec(const string& query) const
{
	return connection_rw_ ? connection_rw_->exec(query) : 0;
}
int database::Context::exec(const char* query) const
{
	return connection_rw_ ? connection_rw_->exec(query) : 0;
}

SQLite::Transaction database::Context::beginTransaction(bool write) const
{
	return SQLite::Transaction(write ? *connection_rw_ : *connection_ro_);
}


// -----------------------------------------------------------------------------
//
// Database Namespace Functions
//
// -----------------------------------------------------------------------------

namespace slade::database
{
// -----------------------------------------------------------------------------
// Creates any missing tables in the SLADE database [db]
// -----------------------------------------------------------------------------
bool createMissingTables(SQLite::Database& db)
{
	// Get slade.pk3 dir with table definition scripts
	auto tables_dir = app::programResource()->dirAtPath("database/tables");
	if (!tables_dir)
	{
		global::error = "Unable to initialize SLADE database: no table definitions in slade.pk3";
		return false;
	}

	for (const auto& entry : tables_dir->entries())
	{
		// Check table exists
		string table_name{ strutil::Path::fileNameOf(entry->name(), false) };
		if (db.tableExists(table_name))
			continue;

		// Doesn't exist, create table
		string sql{ (const char*)entry->data().data(), entry->data().size() };
		try
		{
			db.exec(sql);
			log::info("Created database table {}", table_name);
		}
		catch (const SQLite::Exception& ex)
		{
			global::error = fmt::format("Failed to create database table {}: {}", table_name, ex.what());
			return false;
		}
	}

	return true;
}

// -----------------------------------------------------------------------------
// Creates and initializes a new program database file at [file_path]
// -----------------------------------------------------------------------------
bool createDatabase(const string& file_path)
{
	SQLite::Database db(file_path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
	return createMissingTables(db);
}

// -----------------------------------------------------------------------------
// Updates the program database tables
// -----------------------------------------------------------------------------
bool updateDatabase()
{
	// Create missing tables
	if (!createMissingTables(*db_global.connectionRW()))
		return false;

	return true;
}

// -----------------------------------------------------------------------------
// Copies the template database from slade.pk3 to the temp folder if needed and
// returns the path to it
// -----------------------------------------------------------------------------
string templateDbPath()
{
	if (template_db_path.empty())
	{
		template_db_path = app::path("slade_template.sqlite", app::Dir::Temp);
		fileutil::copyFile(app::path("res/Database/slade.sqlite", app::Dir::Executable), template_db_path);
	}

	return template_db_path;
}
} // namespace slade::database

// -----------------------------------------------------------------------------
// Returns the 'global' database connection context.
// This can only be used from the main thread
// -----------------------------------------------------------------------------
database::Context& database::global()
{
	// Check we are on the main thread
	if (std::this_thread::get_id() != app::mainThreadId())
		log::warning("A non-main thread is requesting the global database connection context");

	return db_global;
}

// -----------------------------------------------------------------------------
// Returns the 'global' read-only database connection, or nullptr if the
// context isn't connected or this isn't called from the main thread
// -----------------------------------------------------------------------------
SQLite::Database* database::connectionRO()
{
	// Check we are on the main thread
	if (std::this_thread::get_id() != app::mainThreadId())
	{
		log::error("Can't get global database connection from non-main thread, use newConnection instead");
		return nullptr;
	}

	return db_global.connectionRO();
}

// -----------------------------------------------------------------------------
// Returns the 'global' read+write database connection, or nullptr if the
// context isn't connected or this isn't called from the main thread
// -----------------------------------------------------------------------------
SQLite::Database* database::connectionRW()
{
	// Check we are on the main thread
	if (std::this_thread::get_id() != app::mainThreadId())
	{
		log::error("Can't get global database connection from non-main thread, use newConnection instead");
		return nullptr;
	}

	return db_global.connectionRW();
}

// -----------------------------------------------------------------------------
// Executes an sql [query] on the database using the given [connection].
// If [connection] is null, the global read+write connection is used.
// Returns the number of rows modified/created by the query, or 0 if the global
// connection context is not connected
// -----------------------------------------------------------------------------
int database::exec(const string& query, SQLite::Database* connection)
{
	if (!connection)
		connection = connectionRW();
	if (!connection)
		return 0;

	return connection->exec(query);
}
int database::exec(const char* query, SQLite::Database* connection)
{
	if (!connection)
		connection = connectionRW();
	if (!connection)
		return 0;

	return connection->exec(query);
}

// -----------------------------------------------------------------------------
// Returns true if the program database file exists
// -----------------------------------------------------------------------------
bool database::fileExists()
{
	return fileutil::fileExists(app::path("slade.sqlite", app::Dir::User));
}

// -----------------------------------------------------------------------------
// Initialises the program database, creating it if it doesn't exist and opening
// the 'global' connection context.
// Returns false if the database couldn't be created or the global context
// failed to open, true otherwise
// -----------------------------------------------------------------------------
bool database::init()
{
	auto db_path = app::path("slade.sqlite", app::Dir::User);

	// Create database if needed
	bool created = false;
	if (!fileutil::fileExists(db_path))
	{
		if (!createDatabase(db_path))
			return false;

		created = true;
	}

	// Open global connections to database (for main thread usage only)
	if (!db_global.open(db_path))
	{
		global::error = "Unable to open global database connections";
		return false;
	}

	// Update the database if needed
	if (!created)
		return updateDatabase();

	return true;
}

// -----------------------------------------------------------------------------
// Closes the global connection context to the database
// -----------------------------------------------------------------------------
void database::close()
{
	db_global.close();
}






CONSOLE_COMMAND(db, 1, false)
{
	auto command = args[0];

	try
	{
		// List tables
		if (command == "tables")
		{
			if (auto db = database::connectionRO())
			{
				SQLite::Statement sql(*db, "SELECT name FROM sqlite_master WHERE type = 'table' ORDER BY name");
				while (sql.executeStep())
					log::console(sql.getColumn(0).getString());
			}
		}

		// Row count of table
		else if (command == "rowcount")
		{
			if (args.size() < 2)
			{
				log::console("No table name given. Usage: db rowcount <tablename>");
				return;
			}

			if (auto db = database::connectionRO())
			{
				SQLite::Statement sql(*db, fmt::format("SELECT COUNT(*) FROM {}", args[1]));
				if (sql.executeStep())
					log::console("{} rows", sql.getColumn(0).getInt());
				else
					log::console("No such table");
			}
		}

		// Reset table from template
		else if (command == "reset")
		{
			if (args.size() < 2)
			{
				log::console("No table name given. Usage: db reset <tablename>");
				return;
			}

			const auto& table = args[1];

			if (auto db = database::connectionRW())
			{
				auto sql_entry = app::programResource()->entryAtPath(fmt::format("database/tables/{}.sql", table));
				if (!sql_entry)
				{
					log::console("Can't find table sql script for {}", table);
					return;
				}

				string sql{ (const char*)sql_entry->data().data(), sql_entry->data().size() };
				db->exec(fmt::format("DROP TABLE IF EXISTS {}", table));
				db->exec(sql);
				log::console("Table {} recreated and reset to default", table);
			}
		}
	}
	catch (const SQLite::Exception& ex)
	{
		log::error(ex.what());
	}
}