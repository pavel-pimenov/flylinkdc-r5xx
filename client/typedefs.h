/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#pragma once

#ifndef DCPLUSPLUS_DCPP_TYPEDEFS_H_
#define DCPLUSPLUS_DCPP_TYPEDEFS_H_

#include <unordered_map>
#include <unordered_set>
#include <string>

#include "debug.h"
#include "forward.h"

using std::wstring;
using std::string;

typedef std::wstring tstring;
typedef std::vector<string> StringList;
typedef std::pair<string, string> StringPair;
typedef std::vector<StringPair> StringPairList;
typedef std::vector<wstring> TStringList;
typedef std::pair<wstring, wstring> TStringPair;
typedef std::vector<TStringPair> TStringPairList;
typedef std::vector<uint8_t> ByteVector;

typedef std::unordered_map<string, string> StringMap;
typedef std::unordered_set<string> StringSet;

#endif /* TYPEDEFS_H_ */
