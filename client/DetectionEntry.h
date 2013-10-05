/*
 * Copyright (C) 2007-2009 adrian_007, adrian-007 on o2 point pl
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

#ifndef RSXPLUSPLUS_DETECTION_ENTRY_H
#define RSXPLUSPLUS_DETECTION_ENTRY_H

#include "Util.h"
#include <deque>

class DetectionEntry
{
	public:
		typedef deque<pair<string, string> > INFMap;
		
		enum
		{
			GREEN = 1,
			YELLOW,
			RED
		};
		
		DetectionEntry() : Id(0), name(Util::emptyString), cheat(Util::emptyString), comment(Util::emptyString), rawToSend(0), clientFlag(1), checkMismatch(false), isEnabled(true) { }
		~DetectionEntry()
		{
			defaultMap.clear();
			adcMap.clear();
			nmdcMap.clear();
		};
		
		uint32_t Id;
		//GETSET(uint32_t, Id, Id);
		INFMap defaultMap;
		INFMap adcMap;
		INFMap nmdcMap;
		
		string name;
		string cheat;
		string comment;
		uint32_t rawToSend;
		uint32_t clientFlag;
		bool checkMismatch;
		bool isEnabled;
		/*
		GETSET(string, name, Name);
		GETSET(string, cheat, Cheat);
		GETSET(string, comment, Comment);
		GETSET(uint32_t, rawToSend, RawToSend);
		GETSET(uint32_t, clientFlag, ClientFlag);
		GETSET(bool, checkMismatch, CheckMismatch);
		GETSET(bool, isEnabled, IsEnabled);
		*/
		
};

#endif // RSXPLUSPLUS_DETECTION_ENTRY_H

/**
 * @file
 * $Id: DetectionEntry.h 86 2008-07-01 21:32:33Z adrian_007 $
 */
