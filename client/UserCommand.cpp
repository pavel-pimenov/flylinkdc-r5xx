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

#include "stdinc.h"
#include "UserCommand.h"
#include "StringTokenizer.h"

StringList UserCommand::getDisplayName() const
{
	StringList l_displayName;
	string name_ = name;
	if (!isSet(UserCommand::FLAG_NOSAVE))
	{
		Util::replace("\\", "/", name_);
	}
	Util::replace("//", "\t", name_);
	const StringTokenizer<string> t(name_, '/');
	l_displayName.reserve(t.getTokens().size());
	for (auto i = t.getTokens().cbegin(), iend = t.getTokens().cend(); i != iend; ++i)
	{
		l_displayName.push_back(*i);
		Util::replace("\t", "/", l_displayName.back());
	}
	return l_displayName;
}
