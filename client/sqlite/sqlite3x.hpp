/*
	Copyright (C) 2004-2005 Cory Nelson

	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	1. The origin of this software must not be misrepresented; you must not
		claim that you wrote the original software. If you use this software
		in a product, an acknowledgment in the product documentation would be
		appreciated but is not required.
	2. Altered source versions must be plainly marked as such, and must not be
		misrepresented as being the original software.
	3. This notice may not be removed or altered from any source distribution.
	
	CVS Info :
		$Author: phrostbyte $
		$Date: 2005/06/16 20:46:40 $
		$Revision: 1.1 $
*/

#ifndef __SQLITE3X_HPP__
#define __SQLITE3X_HPP__

#include <string>
#include <vector>
#include "sqlite3.h"
#include "..\client\Exception.h"

namespace sqlite3x {
	class sqlite3_connection
	{
	private:
		friend class sqlite3_command;
		friend class database_error;

		sqlite3 *db;
		void check_db_open();
	public:
		sqlite3_connection();
		explicit sqlite3_connection(const char *db);
		explicit sqlite3_connection(const wchar_t *db);
		~sqlite3_connection();
		sqlite3 * get_db()
		{
			return db;
		}

		void open(const char *db);
		void open(const wchar_t *db);
		void close();
        int sqlite3_changes() 
		{
			return ::sqlite3_changes(db);
		}
    int sqlite3_get_autocommit()
    {
        return ::sqlite3_get_autocommit(db);
    }
		long long insertid();
		void setbusytimeout(int ms);

		const char* executenonquery(const char *sql);
		void executenonquery(const std::string &sql);
#ifdef SQLITE_USE_UNICODE
		void executenonquery(const wchar_t *sql);
		void executenonquery(const std::wstring &sql);
		int executeint(const wchar_t *sql);
		int executeint(const std::wstring &sql);
		long long executeint64(const wchar_t *sql);
		long long executeint64(const std::wstring &sql);
		double executedouble(const wchar_t *sql);
		double executedouble(const std::wstring &sql);
		std::string executestring(const wchar_t *sql);
		std::string executestring(const std::wstring &sql);
		std::wstring executestring16(const char *sql);
		std::wstring executestring16(const std::string &sql);
		std::wstring executestring16(const wchar_t *sql);
		std::wstring executestring16(const std::wstring &sql);
#endif
		int executeint(const char *sql);
		int executeint(const std::string &sql);
		long long executeint64(const char *sql);
		long long executeint64(const std::string &sql);

#ifndef SQLITE_OMIT_FLOATING_POINT
		double executedouble(const char *sql);
		double executedouble(const std::string &sql);
#endif
		std::string executestring(const char *sql);
		std::string executestring(const std::string &sql);
	};

	class sqlite3_transaction  {
	private:
		sqlite3_connection &con;
		bool intrans;

	public:
		sqlite3_transaction(sqlite3_connection &con, bool start = true);
		~sqlite3_transaction();

		void begin();
		void commit();
		void rollback();
	};
	class sqlite3_reader;
	class sqlite3_command  {
	private:
		friend class sqlite3_reader;

		sqlite3_connection &m_con;
		struct sqlite3_stmt *stmt;
		unsigned int m_refs;
		int  m_argc;
		bool m_no_data_found; //[+]PPA
        void check_no_data_found();
	public:
		sqlite3_command(sqlite3_connection &p_con, const char *sql);
		sqlite3_command(sqlite3_connection &p_con, const std::string &sql);
#ifdef SQLITE_USE_UNICODE
		sqlite3_command(sqlite3_connection &p_con, const wchar_t *sql);
		sqlite3_command(sqlite3_connection &p_con, const std::wstring &sql);
#endif
		~sqlite3_command();
		sqlite3_connection& get_connection()
		{
			return m_con;
		}
		void bind(int index);
		void bind(int index, int data);
		void bind(int index, long long data);
		void bind(int index, unsigned long long data)
		{
			bind(index, (long long) data); // TODO
		}
		void bind(int index, unsigned long data)
		{
			bind(index, (long long)data);
		}
		void bind(int index, unsigned int data)
		{
			bind(index, (long long)data);
		}
		
		bool is_no_data_found() const//[+]PPA
		{
			return m_no_data_found;
		}
    int get_column_count() const
    {
        return m_argc;
    }
#ifndef SQLITE_OMIT_FLOATING_POINT
		void bind(int index, double data);
#endif
		void bind(int index, const char *data, int datalen, sqlite3_destructor_type p_destructor_type /*= SQLITE_STATIC or SQLITE_TRANSIENT*/ );
		void bind(int index, const void *data, int datalen, sqlite3_destructor_type p_destructor_type /*= SQLITE_STATIC or SQLITE_TRANSIENT*/ );
		void bind(int index, const std::string &data, sqlite3_destructor_type p_destructor_type /*= SQLITE_STATIC or SQLITE_TRANSIENT*/ );
#ifdef SQLITE_USE_UNICODE
		void bind(int index, const wchar_t *data, int datalen);
		void bind(int index, const std::wstring &data);
#endif
		sqlite3_reader executereader();
		void executenonquery();
		int executeint();
		long long executeint64();
		long long executeint64_no_throw(); //[+]PPA
#ifndef SQLITE_OMIT_FLOATING_POINT
		double executedouble();
#endif
		std::string executestring();
#ifdef SQLITE_USE_UNICODE
		std::wstring executestring16();
#endif
	};

	class sqlite3_reader {
	private:
		friend class sqlite3_command;

		sqlite3_command *cmd;

		explicit sqlite3_reader(sqlite3_command *cmd);
		void check_reader(int p_index);
	public:
		sqlite3_reader();
		sqlite3_reader(const sqlite3_reader &copy);
		~sqlite3_reader();

		sqlite3_reader& operator=(const sqlite3_reader &copy);

		bool read();
		void reset();
		void close();

		int getint(int index);
		long long getint64(int index);
#ifndef SQLITE_OMIT_FLOATING_POINT
		double getdouble(int index);
#endif
		std::string getstring(int index);
#ifdef SQLITE_USE_UNICODE
		std::wstring getstring16(int index);
		std::wstring getcolname16(int index);
#endif
		void getblob(int index, std::vector<unsigned char>& p_result);
        bool getblob(int index, void* p_result, int p_size);
		std::string getcolname(int index);
	};
	class database_error : public Exception {
	public:
		database_error(const char *msg, const string& p_add_info = ""): Exception(string(msg) + p_add_info) {}
		explicit database_error(const sqlite3_connection* p_con)
			:Exception(sqlite3_errmsg(p_con->db)){}
	};
}

#endif
