/*
 * Copyright (C) 2011-2016 FlylinkDC++ Team http://flylinkdc.com/
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

#ifdef IRAINMAN_INCLUDE_PROVIDER_RESOURCES_AND_CUSTOM_MENU

#if !defined(CUSTOM_MENU_MANAGER_H)
#define CUSTOM_MENU_MANAGER_H

#include "../client/Util.h"
#include "../client/Singleton.h"
#include "../client/SettingsManager.h"

class CustomMenuItem
{
		typedef CustomMenuItem* Ptr;
	public:
		CustomMenuItem(
		    int p_type, int p_id, const string& p_title
		): m_type(p_type), m_id(p_id), m_title(p_title) {}
		CustomMenuItem(const CustomMenuItem* p_src):
			m_id(p_src->getID()), m_title(p_src->getTitle()),  m_type(p_src->getType()) { }
	public:
		GETSET(int, m_id, ID);
		GETSET(string, m_title, Title);
		GETSET(int, m_type, Type);
};

class CustomMenuManager
	: public Singleton<CustomMenuManager>
{
	public:
	
		typedef vector<CustomMenuItem*> MenuList;
		typedef std::map<int, string> URLList;
		
		
		CustomMenuManager(void);
		~CustomMenuManager(void);
		
		const MenuList& getMenuList() const
		{
			return m_menuList;
		}
		
		void load();
		
		const string& GetUrlByID(int id);
		
		GETSET(string, _title, Title);
		
		
	private:
	
		static size_t GetData(const string& url, string& data);
		void clearList();
		
		MenuList m_menuList;
		URLList m_urlList;
		
		
		void ProcessXMLSubMenu(SimpleXML& xml, int& i);
};

#endif // CUSTOM_MENU_MANAGER_H

#endif // IRAINMAN_INCLUDE_PROVIDER_RESOURCES_AND_CUSTOM_MENU