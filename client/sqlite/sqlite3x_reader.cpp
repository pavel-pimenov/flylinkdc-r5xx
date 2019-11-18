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
#include <stdexcept>
#include "sqlite3.h"
#include "sqlite3x.hpp"
#ifdef __linux__
 #include <string.h>
#endif
namespace sqlite3x {

sqlite3_reader::sqlite3_reader() : cmd(NULL) {}

sqlite3_reader::sqlite3_reader(const sqlite3_reader &copy) : cmd(copy.cmd) {
	if(this->cmd) ++this->cmd->m_refs;
}

sqlite3_reader::sqlite3_reader(sqlite3_command *cmd) : cmd(cmd) {
	++cmd->m_refs;
}

sqlite3_reader::~sqlite3_reader() {
	this->close();
}

sqlite3_reader& sqlite3_reader::operator=(const sqlite3_reader &copy) {
	this->close();

	this->cmd=copy.cmd;
	if(this->cmd) ++this->cmd->m_refs;

	return *this;
}

bool sqlite3_reader::read() {
	if(!this->cmd) throw database_error("reader is closed");

	switch(sqlite3_step(this->cmd->stmt)) {
		case SQLITE_ROW:
			return true;
		case SQLITE_DONE:
			return false;
		default:
			throw database_error(&this->cmd->m_con);
	}
}

void sqlite3_reader::reset() {
	if(!this->cmd) throw database_error("reader is closed");

	if(sqlite3_reset(this->cmd->stmt)!=SQLITE_OK)
		throw database_error(&this->cmd->m_con);
}

void sqlite3_reader::close() {
	if(this->cmd) {
		if(--this->cmd->m_refs==0) sqlite3_reset(this->cmd->stmt);
		this->cmd=NULL;
	}
}
void sqlite3_reader::check_reader(int p_index)
{
	if(!cmd) 
		 throw database_error("reader is closed");
	if(p_index > cmd->m_argc-1) 
		 throw std::out_of_range("index out of range");
}

int sqlite3_reader::getint(int index) {
	check_reader(index);
	return sqlite3_column_int(this->cmd->stmt, index);
}

long long sqlite3_reader::getint64(int index) {
	check_reader(index);
	return sqlite3_column_int64(this->cmd->stmt, index);
}

#ifndef SQLITE_OMIT_FLOATING_POINT
double sqlite3_reader::getdouble(int index) {
	check_reader(index);
	return sqlite3_column_double(this->cmd->stmt, index);
}
#endif
std::string sqlite3_reader::getstring(int index) {
	check_reader(index);
    const int l_size = sqlite3_column_bytes(this->cmd->stmt, index);
    if(l_size) 
   	   return std::string((const char*)sqlite3_column_text(this->cmd->stmt, index), l_size);
    else
       return "";   
}

bool sqlite3_reader::getblob(int index, void* p_result, int p_size)
{
	check_reader(index);
  const int l_size = sqlite3_column_bytes(this->cmd->stmt, index);
  if(l_size == p_size)
  {
	const void* l_blob = sqlite3_column_blob(this->cmd->stmt, index);
	if(l_blob)
	{
		memcpy(p_result,l_blob,p_size);
	    return true;
	}
  }
		return false;
}
void sqlite3_reader::getblob(int index, std::vector<unsigned char>& p_result)
{
	check_reader(index);
	const int l_size = sqlite3_column_bytes(this->cmd->stmt, index);
    p_result.resize(l_size);
	if(l_size)
	   memcpy(&p_result[0],sqlite3_column_blob(this->cmd->stmt, index),l_size);
	return;
}

std::string sqlite3_reader::getcolname(int index) {
	check_reader(index);
	return sqlite3_column_name(this->cmd->stmt, index);
}

#ifdef SQLITE_USE_UNICODE
std::wstring sqlite3_reader::getstring16(int index) {
	check_reader(index);
	return std::wstring((const wchar_t*)sqlite3_column_text16(this->cmd->stmt, index), sqlite3_column_bytes16(this->cmd->stmt, index)/2);
}
std::wstring sqlite3_reader::getcolname16(int index) {
	check_reader(index);
	return (const wchar_t*)sqlite3_column_name16(this->cmd->stmt, index);
}
#endif
}
