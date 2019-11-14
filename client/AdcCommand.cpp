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
#include "AdcCommand.h"

AdcCommand::AdcCommand(uint32_t aCmd, char aType /* = TYPE_CLIENT */) : m_cmdInt(aCmd), m_from(0), m_type(aType), m_to(0)
{
	dcassert(m_cmd[3] == 0);
	m_cmd[3] = 0;
}
AdcCommand::AdcCommand(uint32_t aCmd, const uint32_t aTarget, char aType) : m_cmdInt(aCmd), m_from(0), m_to(aTarget), m_type(aType)
{
	dcassert(m_cmd[3] == 0);
	m_cmd[3] = 0;
}
AdcCommand::AdcCommand(Severity sev, Error err, const string& desc, char aType /* = TYPE_CLIENT */) : m_cmdInt(CMD_STA), m_from(0), m_type(aType), m_to(0)
{
	addParam((sev == SEV_SUCCESS && err == SUCCESS) ? "000" : Util::toString(sev * 100 + err));
	addParam(desc);
	dcassert(m_cmd[3] == 0);
	m_cmd[3] = 0;
}

AdcCommand::AdcCommand(const string& aLine, bool nmdc /* = false */) : m_cmdInt(0), m_type(TYPE_CLIENT)
{
	parse(aLine, nmdc);
	dcassert(m_cmd[3] == 0);
	m_cmd[3] = 0;
}

void AdcCommand::parse(const string& aLine, bool nmdc /* = false */)
{
	string::size_type i = 5;
	//m_CID.init();
	if (nmdc)
	{
		// "$ADCxxx ..."
		if (aLine.length() < 7)
			throw ParseException("Too short");
		m_type = TYPE_CLIENT;
		m_cmd[0] = aLine[4];
		m_cmd[1] = aLine[5];
		m_cmd[2] = aLine[6];
		i += 3;
	}
	else
	{
		// "yxxx ..."
		if (aLine.length() < 4)
			throw ParseException("Too short");
		m_type = aLine[0];
		m_cmd[0] = aLine[1];
		m_cmd[1] = aLine[2];
		m_cmd[2] = aLine[3];
	}
	
	if (m_type != TYPE_BROADCAST && m_type != TYPE_CLIENT && m_type != TYPE_DIRECT && m_type != TYPE_ECHO && m_type != TYPE_FEATURE && m_type != TYPE_INFO && m_type != TYPE_HUB && m_type != TYPE_UDP)
	{
		throw ParseException("Invalid type");
	}
	
	if (m_type == TYPE_INFO)
	{
		m_from = HUB_SID;
	}
	
	string::size_type len = aLine.length();
	const char* buf = aLine.c_str();
	string cur;
	cur.reserve(128);
	
	bool toSet = false;
	bool featureSet = false;
	bool fromSet = nmdc; // $ADCxxx never have a from CID...
	
	while (i < len)
	{
		switch (buf[i])
		{
			case '\\':
				++i;
				if (i == len)
					throw ParseException("Escape at eol");
				if (buf[i] == 's')
					cur += ' ';
				else if (buf[i] == 'n')
					cur += '\n';
				else if (buf[i] == '\\')
					cur += '\\';
				else if (buf[i] == ' ' && nmdc) // $ADCGET escaping, leftover from old specs
					cur += ' ';
				else
					throw ParseException("Unknown escape");
				break;
			case ' ':
				// New parameter...
			{
				if ((m_type == TYPE_BROADCAST || m_type == TYPE_DIRECT || m_type == TYPE_ECHO || m_type == TYPE_FEATURE) && !fromSet)
				{
					if (cur.length() != 4)
					{
						throw ParseException("Invalid SID length");
					}
					m_from = toSID(cur);
					fromSet = true;
				}
				else if ((m_type == TYPE_DIRECT || m_type == TYPE_ECHO) && !toSet)
				{
					if (cur.length() != 4)
					{
						throw ParseException("Invalid SID length");
					}
					m_to = toSID(cur);
					toSet = true;
				}
				else if (m_type == TYPE_FEATURE && !featureSet)
				{
					if (cur.length() % 5 != 0)
					{
						throw ParseException("Invalid feature length");
					}
					// Skip...
					featureSet = true;
				}
				else
				{
					parameters.push_back(cur);
					/*
					if (cur.size() > 2 && cur[0] == 'I' && cur[1] == 'D')
					{
					    if (cur.size() == 41)
					    {
					        m_CID = CID(cur.substr(2));
					    }
					}
					*/
				}
				cur.clear();
			}
			break;
			default:
				cur += buf[i];
		}
		++i;
	}
	if (!cur.empty())
	{
		if ((m_type == TYPE_BROADCAST || m_type == TYPE_DIRECT || m_type == TYPE_ECHO || m_type == TYPE_FEATURE) && !fromSet)
		{
			if (cur.length() != 4)
			{
				throw ParseException("Invalid SID length");
			}
			m_from = toSID(cur);
			fromSet = true;
		}
		else if ((m_type == TYPE_DIRECT || m_type == TYPE_ECHO) && !toSet)
		{
			if (cur.length() != 4)
			{
				throw ParseException("Invalid SID length");
			}
			m_to = toSID(cur);
			toSet = true;
		}
		else if (m_type == TYPE_FEATURE && !featureSet)
		{
			if (cur.length() % 5 != 0)
			{
				throw ParseException("Invalid feature length");
			}
			// Skip...
			featureSet = true;
		}
		else
		{
			parameters.push_back(cur);
		}
	}
	
	if ((m_type == TYPE_BROADCAST || m_type == TYPE_DIRECT || m_type == TYPE_ECHO || m_type == TYPE_FEATURE) && !fromSet)
	{
		throw ParseException("Missing from_sid");
	}
	
	if (m_type == TYPE_FEATURE && !featureSet)
	{
		throw ParseException("Missing feature");
	}
	
	if ((m_type == TYPE_DIRECT || m_type == TYPE_ECHO) && !toSet)
	{
		throw ParseException("Missing to_sid");
	}
}

string AdcCommand::toString(const CID& aCID, bool nmdc /* = false */) const
{
	return getHeaderString(aCID) + getParamString(nmdc);
}

string AdcCommand::toString(uint32_t sid /* = 0 */, bool nmdc /* = false */) const
{
	return getHeaderString(sid, nmdc) + getParamString(nmdc);
}

string AdcCommand::escape(const string& str, bool old)
{
	string tmp = str;
	string::size_type i = 0;
	while ((i = tmp.find_first_of(" \n\\", i)) != string::npos)
	{
		if (old)
		{
			tmp.insert(i, "\\");
		}
		else
		{
			switch (tmp[i])
			{
				case ' ':
					tmp.replace(i, 1, "\\s");
					break;
				case '\n':
					tmp.replace(i, 1, "\\n");
					break;
				case '\\':
					tmp.replace(i, 1, "\\\\");
					break;
			}
		}
		i += 2;
	}
	return tmp;
}

string AdcCommand::getHeaderString(uint32_t sid, bool nmdc) const
{
	string tmp;
	if (nmdc)
	{
		tmp += "$ADC";
	}
	else
	{
		tmp += getType();
	}
	
	tmp += m_cmdChar;
	
	if (m_type == TYPE_BROADCAST || m_type == TYPE_DIRECT || m_type == TYPE_ECHO || m_type == TYPE_FEATURE)
	{
		tmp += ' ';
		tmp += fromSID(sid);
	}
	
	if (m_type == TYPE_DIRECT || m_type == TYPE_ECHO)
	{
		tmp += ' ';
		tmp += fromSID(m_to);
	}
	
	if (m_type == TYPE_FEATURE)
	{
		tmp += ' ';
		tmp += features;
	}
	return tmp;
}

string AdcCommand::getHeaderString(const CID& cid) const
{
	dcassert(m_type == TYPE_UDP);
	string tmp;
	tmp.reserve(44);
	tmp += getType();
	tmp += m_cmdChar;
	tmp += ' ';
	tmp += cid.toBase32();
	return tmp;
}

string AdcCommand::getParamString(bool nmdc) const
{
	string tmp;
	tmp.reserve(65);
	for (auto i = getParameters().cbegin(); i != getParameters().cend(); ++i)
	{
		tmp += ' ';
		tmp += escape(*i, nmdc);
	}
	if (nmdc)
	{
		tmp += '|';
	}
	else
	{
		tmp += '\n';
	}
	return tmp;
}

bool AdcCommand::getParam(const char* name, size_t start, string& ret) const
{
	for (string::size_type i = start; i < getParameters().size(); ++i)
	{
		if (toCode(name) == toCode(getParameters()[i].c_str()))
		{
			ret = getParameters()[i].substr(2);
			return true;
		}
	}
	return false;
}

bool AdcCommand::hasFlag(const char* name, size_t start) const
{
	for (string::size_type i = start; i < getParameters().size(); ++i)
	{
		if (toCode(name) == toCode(getParameters()[i].c_str()) &&
		        getParameters()[i].size() == 3 &&
		        getParameters()[i][2] == '1')
		{
			return true;
		}
	}
	return false;
}

/**
 * @file
 * $Id: AdcCommand.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
