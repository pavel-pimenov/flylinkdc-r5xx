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

#include "SimpleXML.h"
#include "ResourceManager.h"

const string SimpleXML::utf8Header = "<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"yes\"?>\r\n";

string& SimpleXML::escape(string& aString, bool aAttrib, bool aLoading /* = false */, const string &encoding /* = "UTF-8" */)
{
	string::size_type i = 0;
	const char* chars = aAttrib ? "<&>'\"" : "<&>";
	
	if (aLoading)
	{
		while ((i = aString.find('&', i)) != string::npos)
		{
			if (aString.compare(i + 1, 3, "lt;", 3) == 0)
			{
				aString.replace(i, 4, 1, '<');
			}
			else if (aString.compare(i + 1, 4, "amp;", 4) == 0)
			{
				aString.replace(i, 5, 1, '&');
			}
			else if (aString.compare(i + 1, 3, "gt;", 3) == 0)
			{
				aString.replace(i, 4, 1, '>');
			}
			else if (aAttrib)
			{
				if (aString.compare(i + 1, 5, "apos;", 5) == 0)
				{
					aString.replace(i, 6, 1, '\'');
				}
				else if (aString.compare(i + 1, 5, "quot;", 5) == 0)
				{
					aString.replace(i, 6, 1, '"');
				}
			}
			i++;
		}
		i = 0;
		if ((i = aString.find('\n')) != string::npos)
		{
			if (i > 0 && aString[i - 1] != '\r')
			{
				// This is a unix \n thing...convert it...
				i = 0;
				while ((i = aString.find('\n', i)) != string::npos)
				{
					if (aString[i - 1] != '\r')
						aString.insert(i, 1, '\r');
						
					i += 2;
				}
			}
		}
		aString = Text::toUtf8(aString, encoding);
	}
	else
	{
		while ((i = aString.find_first_of(chars, i)) != string::npos)
		{
			switch (aString[i])
			{
				case '<':
					aString.replace(i, 1, "&lt;");
					i += 4;
					break;
				case '&':
					aString.replace(i, 1, "&amp;");
					i += 5;
					break;
				case '>':
					aString.replace(i, 1, "&gt;");
					i += 4;
					break;
				case '\'':
					aString.replace(i, 1, "&apos;");
					i += 6;
					break;
				case '"':
					aString.replace(i, 1, "&quot;");
					i += 6;
					break;
				default:
					dcassert(0);
			}
		}
		// No need to convert back to acp since our utf8Header denotes we
		// should store it as utf8.
	}
	return aString;
}

void SimpleXML::Tag::appendAttribString(string& tmp)
{
	for (auto i = attribs.cbegin(); i != attribs.cend(); ++i)
	{
		tmp.append(i->first);
		tmp.append("=\"", 2);
		if (needsEscape(i->second, true))
		{
			string tmp2(i->second);
			escape(tmp2, true);
			tmp.append(tmp2);
		}
		else
		{
			tmp.append(i->second);
		}
		tmp.append("\" ", 2);
	}
	tmp.erase(tmp.size() - 1);
}

/**
 * The same as the version above, but writes to a file instead...yes, this could be made
 * with streams and only one code set but streams are slow...the file f should be a buffered
 * file, otherwise things will be very slow (I assume write is not expensive and call it a lot
 */
void SimpleXML::Tag::toXML(int indent, OutputStream* f)
{
	if (children.empty() && data.empty())
	{
		string tmp;
		tmp.reserve(indent + name.length() + 30);
		tmp.append(indent, '\t');
		tmp.append(1, '<');
		tmp.append(name);
		tmp.append(1, ' ');
		appendAttribString(tmp);
		tmp.append("/>\r\n", 4);
		f->write(tmp);
	}
	else
	{
		string tmp;
		tmp.append(indent, '\t');
		tmp.append(1, '<');
		tmp.append(name);
		tmp.append(1, ' ');
		appendAttribString(tmp);
		if (children.empty())
		{
			tmp.append(1, '>');
			if (needsEscape(data, false))
			{
				string tmp2(data);
				escape(tmp2, false);
				tmp.append(tmp2);
			}
			else
			{
				tmp.append(data);
			}
		}
		else
		{
			tmp.append(">\r\n", 3);
			f->write(tmp);
			tmp.clear();
			for (auto i = children.cbegin(); i != children.cend(); ++i)
			{
				(*i)->toXML(indent + 1, f);
			}
			tmp.append(indent, '\t');
		}
		tmp.append("</", 2);
		tmp.append(name);
		tmp.append(">\r\n", 3);
		f->write(tmp);
	}
}

bool SimpleXML::findChild(const string& aName) noexcept
{
	dcassert(current != NULL);
	if (!current)
		return false;
		
	if (found && currentChild != current->children.end())
		++currentChild;
		
	while (currentChild != current->children.end())
	{
		if ((*currentChild)->name == aName) // https://www.box.net/shared/8622670a39f3c10523e0
		{
			found = true;
			return true;
		}
		else
			++currentChild;
	}
	return false;
}

void SimpleXML::addTag(const string& aName, const string& aData /* = "" */)
{
	if (aName.empty())
	{
		throw SimpleXMLException(STRING(SXML_EMPTY_TAG_NAME));
	}
	
	if (current == &root && !current->children.empty())
	{
		throw SimpleXMLException(STRING(SXML_ONLE_ONE_ROOT));
	}
	else
	{
		current->children.push_back(new Tag(aName, aData, current));
		currentChild = current->children.end() - 1;
	}
}

void SimpleXML::addAttrib(const string& aName, const string& aData)
{
	if (current == &root)
		throw SimpleXMLException(STRING(SXML_NO_TAG_SELECTED));
		
	current->attribs.push_back(make_pair(aName, aData));
}

void SimpleXML::addChildAttrib(const string& aName, const string& aData)
{
	checkChildSelected();
	
	(*currentChild)->attribs.push_back(make_pair(aName, aData));
}

void SimpleXML::fromXML(const string& aXML)
{
	if (!root.children.empty())
	{
		delete root.children[0];
		root.children.clear();
	}
	
	TagReader t(&root);
	SimpleXMLReader(&t).parse(aXML.c_str(), aXML.size(), false);
	
	if (root.children.size() != 1)
	{
		throw SimpleXMLException(STRING(SXML_INVALID_FILE));
	}
	
	current = &root;
	resetCurrentChild();
}
void SimpleXML::stepIn()
{
	checkChildSelected();
	current = *currentChild;
	currentChild = current->children.begin();
	found = false;
}

void SimpleXML::stepOut()
{
	if (current == &root)
		throw SimpleXMLException("Already at lowest level");
		
	dcassert(current && current->parent);
	if (!current)
		return;
	if (!current->parent)
		return;
	currentChild = find(current->parent->children.begin(), current->parent->children.end(), current);
	
	current = current->parent;
	found = true;
}

void SimpleXML::resetCurrentChild()
{
	found = false;
	dcassert(current != NULL);
	if (!current)
		return;
		
	currentChild = current->children.begin();
}

string SimpleXML::getChildAttribTrim(const string& aName, const string& aDefault /*= Util::emptyString*/) const
{
	string l_trim_val = getChildAttrib(aName, aDefault);
	Text::trim(l_trim_val);
	return l_trim_val;
}

/**
 * @file
 * $Id: SimpleXML.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
