/*
 * Copyright (C) 2001-2013 Jacek Sieka, arnetheduck on gmail point com
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

#ifndef DCPLUSPLUS_DCPP_SIMPLE_XML_H
#define DCPLUSPLUS_DCPP_SIMPLE_XML_H

#include "File.h"
#include "SimpleXMLReader.h"
#include <boost/algorithm/string/trim.hpp>
#include "StringTokenizer.h"

STANDARD_EXCEPTION(SimpleXMLException);

/**
 * A simple XML class that loads an XML-ish structure into an internal tree
 * and allows easy access to each element through a "current location".
 */
class SimpleXML
#ifdef _DEBUG
	: private boost::noncopyable
#endif
{
	public:
		SimpleXML() : root("BOGUSROOT", Util::emptyString, NULL), current(&root), found(false)
		{
			resetCurrentChild();
		}
		~SimpleXML() { }
		
		void addTag(const string& aName, const string& aData = Util::emptyString);
		void addTag(const string& aName, int aData)
		{
			addTag(aName, Util::toString(aData));
		}
		void addTag(const string& aName, int64_t aData)
		{
			addTag(aName, Util::toString(aData));
		}
		
		template<typename T>
		void addAttrib(const string& aName, const T& aData)
		{
			addAttrib(aName, Util::toString(aData));
		}
		
		void addAttrib(const string& aName, const string& aData);
		void addAttrib(const string& aName, bool aData)
		{
			addAttrib(aName, string(aData ? "1" : "0"));
		}
		
		template <typename T>
		void addChildAttrib(const string& aName, const T& aData)
		{
			addChildAttrib(aName, Util::toString(aData));
		}
		void addChildAttrib(const string& aName, const string& aData);
		void addChildAttribIfNotEmpty(const string& aName, const string& aData)
		{
			if (!aData.empty())
			{
				addChildAttrib(aName, aData);
			}
		}
		void addChildAttrib(const string& aName, bool aData)
		{
			addChildAttrib(aName, string(aData ? "1" : "0"));
		}
		
		const string& getData() const
		{
			dcassert(current != NULL);
			return current->data;
		}
		
		void stepIn()
		{
			checkChildSelected();
			current = *currentChild;
			currentChild = current->children.begin();
			found = false;
		}
		
		void stepOut()
		{
			if (current == &root)
				throw SimpleXMLException("Already at lowest level");
				
			dcassert(current->parent != NULL);
			if (!current->parent)
				return;
			currentChild = find(current->parent->children.begin(), current->parent->children.end(), current);
			
			current = current->parent;
			found = true;
		}
		
		void resetCurrentChild() noexcept
		{
			found = false;
			dcassert(current != NULL);
			if (!current)
				return;
				
			currentChild = current->children.begin();
		}
		
		bool findChild(const string& aName) noexcept;
		
		const string& getChildData() const
		{
			checkChildSelected();
			return (*currentChild)->data;
		}
		
		const string& getChildAttrib(const string& aName, const string& aDefault = Util::emptyString) const
		{
			checkChildSelected();
			return (*currentChild)->getAttrib(aName, aDefault);
		}
		int getIntChildAttrib(const string& aName,  const string& aDefault) const
		{
			checkChildSelected();
			return Util::toInt(getChildAttrib(aName, aDefault));
		}
		int getIntChildAttrib(const string& aName) const
		{
			checkChildSelected();
			return Util::toInt(getChildAttrib(aName));
		}
		int getInt64ChildAttrib(const string& aName,  const string& aDefault) const // [+] IRainman fix.
		{
			checkChildSelected();
			return Util::toInt64(getChildAttrib(aName, aDefault));
		}
		int getInt64ChildAttrib(const string& aName) const // [+] IRainman fix.
		{
			checkChildSelected();
			return Util::toInt64(getChildAttrib(aName));
		}
		string getChildAttribTrim(const string& aName, const string& aDefault = Util::emptyString) const //[+]PPA
		{
			string l_trim_val = getChildAttrib(aName, aDefault);
			boost::algorithm::trim(l_trim_val);
			return l_trim_val;
		}
		template<class T>
		void getChildAttribSplit(const string& aName,
		                         T& aCollection,
		                         std::function<void(const string&)> inserter,
		                         const string& aDefault = Util::emptyString, // Не юзаются. зачем?
		                         const char aToken = ',' // Не юзаются. зачем?
		                        ) const // [+] IRainman
		{
			const StringTokenizer<string> tokinizer(getChildAttrib(aName, aDefault), aToken);
			const auto& tokens = tokinizer.getTokens();
			aCollection.clear();
			/* TODO
			if (std::is_member_function_pointer<decltype(&T::reserve)>::value)
			    aCollection.reserve(tokens.size());
			    */
			for_each(tokens.cbegin(), tokens.cend(), inserter);
		}
		int64_t getLongLongChildAttrib(const string& aName) const
		{
			checkChildSelected();
			return Util::toInt64(getChildAttrib(aName));
		}
		bool getBoolChildAttrib(const string& aName) const
		{
			checkChildSelected();
			const string& tmp = getChildAttrib(aName);
			
			return !tmp.empty() && tmp[0] == '1';
		}
		
		void fromXML(const string& aXML);
		string toXML()
		{
			string tmp;
			StringOutputStream os(tmp);
			toXML(&os);
			return tmp;
		}
		void toXML(OutputStream* f)
		{
			if (!root.children.empty())
				root.children[0]->toXML(0, f);
		}
		
		static const string& escapeAtrib(const string& p_str, string& p_tmp) // [+]PPA
		{
			if (needsEscapeForce(p_str))
			{
				p_tmp = p_str;
				return escape(p_tmp, true);
			}
			return p_str;
		}
		static const string& escape(const string& str, string& tmp, bool aAttrib, bool aLoading = false, const string &encoding = Text::g_utf8)
		{
			if (needsEscape(str, aAttrib, aLoading, encoding))
			{
				tmp = str;
				return escape(tmp, aAttrib, aLoading, encoding);
			}
			return str;
		}
		inline static const string& escapeForce(const string& p_str, string& p_tmp) //[+]PPA
		{
			p_tmp = p_str;
			return escape(p_tmp, true);
		}
		
		static string& escape(string& aString, bool aAttrib, bool aLoading = false, const string &encoding = Text::g_utf8);
		/**
		 * This is a heuristic for whether escape needs to be called or not. The results are
		 * only guaranteed for false, i e sometimes true might be returned even though escape
		 * was not needed...
		 */
		inline static bool needsEscape(const string& aString, bool aAttrib, bool aLoading = false, const string &encoding = Text::g_utf8)
		{
			const bool l_is_utf8 = stricmp(encoding, Text::g_utf8) == 0;
			return !l_is_utf8 || ((aLoading ? aString.find('&') : aString.find_first_of(aAttrib ? "<&>'\"" : "<&>")) != string::npos);
		}
		inline static bool needsEscapeForce(const string& aString) // [+]PPA
		{
			return aString.find_first_of("<&>'\"") != string::npos;
		}
		static const string utf8Header;
	private:
		class Tag
#ifdef _DEBUG
			: boost::noncopyable // [+] IRainman fix.
#endif
		{
			public:
				typedef Tag* Ptr;
				typedef vector<Ptr> List;
				typedef List::const_iterator Iter;
				
				/**
				 * A simple list of children. To find a tag, one must search the entire list.
				 */
				List children;
				/**
				 * Attributes of this tag. According to the XML standard the names
				 * must be unique (case-sensitive). (Assuming that we have few attributes here,
				 * we use a vector instead of a (hash)map to save a few bytes of memory and unnecessary
				 * calls to the memory allocator...)
				 */
				StringPairList attribs;
				
				/** Tag name */
				string name;
				
				/** Tag data, may be empty. */
				string data;
				
				/** Parent tag, for easy traversal */
				Ptr parent;
				
				Tag(const string& aName, const StringPairList& a, Ptr aParent) : attribs(a), name(aName), data(), parent(aParent)
				{
				}
				
				Tag(const string& aName, const string& d, Ptr aParent) : name(aName), data(d), parent(aParent)
				{
				}
				
				const string& getAttrib(const string& aName, const string& aDefault = Util::emptyString) const
				{
					StringPairList::const_iterator i = find_if(attribs.begin(), attribs.end(), CompareFirst<string, string>(aName));
					return (i == attribs.end()) ? aDefault : i->second;
				}
				void toXML(int indent, OutputStream* f);
				
				void appendAttribString(string& tmp);
				/** Delete all children! */
				~Tag()
				{
					for (auto i = children.cbegin(); i != children.cend(); ++i)
					{
						delete *i;
					}
				}
		};
		
		class TagReader : public SimpleXMLReader::CallBack
		{
			public:
				TagReader(Tag* root) : cur(root) { }
				virtual bool getData(string&) const
				{
					return false;
				}
				virtual void startTag(const string& name, StringPairList& attribs, bool simple)
				{
					cur->children.push_back(new Tag(name, attribs, cur));
					if (!simple)
						cur = cur->children.back();
				}
				virtual void endTag(const string&, const string& d)
				{
					cur->data = d;
					if (cur->parent == NULL)
						throw SimpleXMLException("Invalid end tag");
					cur = cur->parent;
				}
				
				Tag* cur;
		};
		
		/** Bogus root tag, should have only one child! */
		Tag root;
		
		/** Current position */
		Tag::Ptr current;
		
		Tag::Iter currentChild;
		
		void checkChildSelected() const noexcept
		{
		    dcassert(current != NULL);
		    if (!current)
		    return;
		    dcassert(currentChild != current->children.end());
		}
		
		bool found;
};

#endif // DCPLUSPLUS_DCPP_SIMPLE_XML_H

/**
 * @file
 * $Id: SimpleXML.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
