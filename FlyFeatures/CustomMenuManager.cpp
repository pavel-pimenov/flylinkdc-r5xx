/*
 * Copyright (C) 2011-2012 FlylinkDC++ Team http://flylinkdc.com/
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

#ifdef IRAINMAN_INCLUDE_PROVIDER_RESOURCES_AND_CUSTOM_MENU

#include <wininet.h>
#include "CustomMenuManager.h"
#include "SimpleXML.h"
#include "LogManager.h"
//#define FRAME_BUFFER_SIZE 4096


CustomMenuManager::CustomMenuManager(void)
{
	SettingsManager::getInstance()->addListener(this);
}

CustomMenuManager::~CustomMenuManager(void)
{
	clearList();
	SettingsManager::getInstance()->removeListener(this);
}

void
CustomMenuManager::clearList()
{
	for (auto i = m_menuList.cbegin(); i != m_menuList.cend(); ++i)
	{
		delete *i;
	}
	m_menuList.clear();
	m_urlList.clear();
}

void CustomMenuManager::load(/* SimpleXML&*/ /*aXml*/)
{
	clearList();
	string data;
	if (BOOLSETTING(PROVIDER_USE_RESOURCES))
	{
		string strRootProviderURL = SETTING(ISP_RESOURCE_ROOT_URL);
		if (!strRootProviderURL.empty())
		{
			AppendUriSeparator(strRootProviderURL);
			string strCustomMenu = strRootProviderURL + "ISP_menu.xml";
			GetData(strCustomMenu, data);
		}
	}
	
	// [!] SSA use old way for compatibility
	if (data.empty() && SETTING(USE_CUSTOM_MENU))
	{
		const string& strPathURL = SETTING(CUSTOM_MENU_PATH);
		if (!strPathURL.empty())
		{
			GetData(strPathURL, data);
		}
	}
	
	if (!data.empty())
	{
		SimpleXML xml;
		int i = 0;
		try
		{
			// Download and Parse it.
			xml.fromXML(data);
			xml.resetCurrentChild();
			if (xml.findChild("CustomMenu"))
			{
				setTitle(xml.getChildAttrib("name"));
				xml.stepIn();
				ProcessXMLSubMenu(xml, i);
			}
		}
		catch (const Exception& e)
		{
			LogManager::getInstance()->message(e.getError());
			clearList();
		}
	}
	
}
void
CustomMenuManager::ProcessXMLSubMenu(SimpleXML& xml, int& i)
{
	while (xml.findChild("MenuItem"))
	{
		const string& type = xml.getChildAttrib("type");
		if (!type.empty())
		{
			if (type == "menuItem")
			{
				const string& url = xml.getChildAttrib("link");
				const string& title = xml.getChildData();
				CustomMenuItem* item = new CustomMenuItem(1, i, title);
				m_urlList[i++] = url;
				m_menuList.push_back(item);
			}
			else if (type == "separator")
			{
				CustomMenuItem* item = new CustomMenuItem(0, 0, Util::emptyString);
				m_menuList.push_back(item);
			}
			else if (type == "submenu")
			{
				const string& title = xml.getChildAttrib("name");
				CustomMenuItem* item = new CustomMenuItem(2, 0, Util::emptyString); // Start Submenu
				m_menuList.push_back(item);
				// Send TO iterations and get items inside.
				xml.stepIn();
				ProcessXMLSubMenu(xml, i);
				xml.stepOut();
				item = new CustomMenuItem(3, 0, title); // End Submenu
				m_menuList.push_back(item);
			}
		}
	}
}
size_t
CustomMenuManager::GetData(const string& url, string& data) const
{
	return Util::getDataFromInet(url, data, 0);
}

const string& CustomMenuManager::GetUrlByID(int id)
{
	if (id > -1 && id < (int)m_urlList.size())
	{
		return m_urlList[id];
	}
	return Util::emptyString;
}

#endif // IRAINMAN_INCLUDE_PROVIDER_RESOURCES_AND_CUSTOM_MENU