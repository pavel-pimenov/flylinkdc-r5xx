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

#include "stdinc.h"
#include "sqlite3.h"
#include "sqlite3x.hpp"

namespace sqlite3x {

sqlite3_command::sqlite3_command(sqlite3_connection &p_con, const char *sql) : m_con(p_con),m_refs(0) {
	const char *tail=NULL;
	if(sqlite3_prepare_v2(p_con.db, sql, -1, &this->stmt, &tail)!=SQLITE_OK)
		throw database_error(&p_con);

	m_argc=sqlite3_column_count(this->stmt);
}

sqlite3_command::sqlite3_command(sqlite3_connection &p_con, const std::string &sql) : m_con(p_con),m_refs(0) {
	const char *tail=NULL;
	if(sqlite3_prepare_v2(p_con.db, sql.data(), (int)sql.length(), &this->stmt, &tail)!=SQLITE_OK)
		throw database_error(&p_con);

	m_argc=sqlite3_column_count(this->stmt);
}

#ifdef SQLITE_USE_UNICODE
sqlite3_command::sqlite3_command(sqlite3_connection &p_con, const wchar_t *sql) : m_con(p_con),refs(0) {
	const wchar_t *tail=NULL;
	if(sqlite3_prepare16_v2(p_con.db, sql, -1, &this->stmt, (const void**)&tail)!=SQLITE_OK)
		throw database_error(&p_con);

	m_argc=sqlite3_column_count(this->stmt);
}

sqlite3_command::sqlite3_command(sqlite3_connection &p_con, const std::wstring &sql) : m_con(p_con),refs(0) {
	const wchar_t *tail=NULL;
	if(sqlite3_prepare16_v2(p_con.db, sql.data(), (int)sql.length()*2, &this->stmt, (const void**)&tail)!=SQLITE_OK)
		throw database_error(&p_con);

	m_argc=sqlite3_column_count(this->stmt);
}
#endif
sqlite3_command::~sqlite3_command() {
	sqlite3_finalize(this->stmt);
}

void sqlite3_command::bind(int index) {
	if(sqlite3_bind_null(this->stmt, index)!=SQLITE_OK)
		throw database_error(&m_con);
}

void sqlite3_command::bind(int index, int data) {
	if(sqlite3_bind_int(this->stmt, index, data)!=SQLITE_OK)
		throw database_error(&m_con);
}

void sqlite3_command::bind(int index, long long data) {
	if(sqlite3_bind_int64(this->stmt, index, data)!=SQLITE_OK)
		throw database_error(&m_con);
}

#ifndef SQLITE_OMIT_FLOATING_POINT
void sqlite3_command::bind(int index, double data) {
	if(sqlite3_bind_double(this->stmt, index, data)!=SQLITE_OK)
		throw database_error(&m_con);
}
#endif

void sqlite3_command::bind(int index, const char *data, int datalen, sqlite3_destructor_type p_destructor_type /*= SQLITE_STATIC  or SQLITE_TRANSIENT*/ ) {
	if(sqlite3_bind_text(this->stmt, index, data, datalen, p_destructor_type)!=SQLITE_OK)
		throw database_error(&m_con);
}


void sqlite3_command::bind(int index, const void *data, int datalen, sqlite3_destructor_type p_destructor_type /*= SQLITE_STATIC  or SQLITE_TRANSIENT*/ ) {
	if(sqlite3_bind_blob(this->stmt, index, data, datalen, p_destructor_type)!=SQLITE_OK)
		throw database_error(&m_con);
}

void sqlite3_command::bind(int index, const std::string &data, sqlite3_destructor_type p_destructor_type /*= SQLITE_STATIC  or SQLITE_TRANSIENT*/) {
	if(sqlite3_bind_text(this->stmt, index, data.data(), (int)data.length(), p_destructor_type)!=SQLITE_OK)
		throw database_error(&m_con);
}
#ifdef SQLITE_USE_UNICODE
void sqlite3_command::bind(int index, const wchar_t *data, int datalen) {
	if(sqlite3_bind_text16(this->stmt, index, data, datalen, SQLITE_STATIC)!=SQLITE_OK)
		throw database_error(&m_con);
}
void sqlite3_command::bind(int index, const std::wstring &data) {
	if(sqlite3_bind_text16(this->stmt, index, data.data(), (int)data.length()*2, SQLITE_STATIC)!=SQLITE_OK)
		throw database_error(&m_con);
}
#endif
sqlite3_reader sqlite3_command::executereader() {
	return sqlite3_reader(this);
}

void sqlite3_command::executenonquery() {
	this->executereader().read();
}
void sqlite3_command::check_no_data_found()
{
	if(m_no_data_found) 
		throw database_error("nothing to read");
}

int sqlite3_command::executeint() {
	sqlite3_reader reader=this->executereader();
	m_no_data_found = !reader.read();
	check_no_data_found();
	return reader.getint(0);
}

long long sqlite3_command::executeint64() {
	sqlite3_reader reader=this->executereader();
	m_no_data_found = !reader.read();
    check_no_data_found();
	return reader.getint64(0);
}
long long sqlite3_command::executeint64_no_throw() {
	sqlite3_reader reader=this->executereader();
	m_no_data_found = !reader.read();
	if(m_no_data_found)
		return 0;
	else
	    return reader.getint64(0);
}

#ifndef SQLITE_OMIT_FLOATING_POINT
double sqlite3_command::executedouble() {
	sqlite3_reader reader=this->executereader();
	m_no_data_found = !reader.read();
	check_no_data_found();
	return reader.getdouble(0);
}
#endif
std::string sqlite3_command::executestring() {
	sqlite3_reader reader=this->executereader();
	m_no_data_found = !reader.read();
	check_no_data_found();
	return reader.getstring(0);
}
#ifdef SQLITE_USE_UNICODE
std::wstring sqlite3_command::executestring16() {
	sqlite3_reader reader=this->executereader();
	m_no_data_found = !reader.read();
	check_no_data_found();
	return reader.getstring16(0);
}
#endif
}
