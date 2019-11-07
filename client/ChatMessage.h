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


#ifndef DCPLUSPLUS_DCPP_CHAT_MESSAGE_H
#define DCPLUSPLUS_DCPP_CHAT_MESSAGE_H

#include "forward.h"

class ChatMessage
#ifdef _DEBUG
	: public boost::noncopyable
#endif
	
{
	public:
		string m_text;
		OnlineUserPtr m_from;
		OnlineUserPtr m_to;
		OnlineUserPtr m_replyTo;
		time_t m_timestamp; // TODO - разобраться когда оно нужно
		bool thirdPerson;
		
		// [+] IRainman fix.
		ChatMessage(const string& _text, const OnlineUserPtr& _from, const OnlineUserPtr& _to = nullptr, const OnlineUserPtr& _replyTo = nullptr, bool _thirdPerson = false)
			: m_text(_text), m_from(_from), m_to(_to), m_replyTo(_replyTo), thirdPerson(_thirdPerson), m_timestamp(0)
		{
		}
		bool isPrivate() const
		{
			return m_to && m_replyTo;
		}
		static string formatNick(const string& nick, const bool thirdPerson)
		{
			// let's *not* obey the spec here and add a space after the star. :P
			return thirdPerson ? "* " + nick + ' ' : '<' + nick + "> ";
		}
		void translate_me()
		{
			if (m_text.size() >= 4 && (strnicmp(m_text.c_str(), "/me ", 4) == 0 ||
			                           strnicmp(m_text.c_str(), "+me ", 4) == 0))
			{
				thirdPerson = true;
				m_text = m_text.substr(4);
			}
		}
		// [~] IRainman fix.
		string format() const;
};

#endif // !defined(DCPLUSPLUS_DCPP_CHAT_MESSAGE_H)
