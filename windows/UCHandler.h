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

#if !defined(UC_HANDLER_H)
#define UC_HANDLER_H


#pragma once

#include "../client/FavoriteManager.h"
#include "../client/ClientManager.h"
#include "OMenu.h"

template<class T>
class UCHandler
{
	public:
		UCHandler() : m_menuPos(0), m_extraItems(0)
		{
			subMenu.CreatePopupMenu();
		}
		
		typedef UCHandler<T> thisClass;
		BEGIN_MSG_MAP(thisClass)
		COMMAND_RANGE_HANDLER(IDC_USER_COMMAND, IDC_USER_COMMAND + m_userCommands.size(), onUserCommand)
		END_MSG_MAP()
		
		LRESULT onUserCommand(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			dcassert(wID >= IDC_USER_COMMAND);
			size_t n = (size_t)wID - IDC_USER_COMMAND;
			dcassert(n < m_userCommands.size());
			
			UserCommand& uc = m_userCommands[n];
			
			T* t = static_cast<T*>(this);
			
			t->runUserCommand(uc);
			return 0;
		}
		
		void appendUcMenu(CMenu& menu, int ctx, const string& hubUrl)
		{
			appendUcMenu(menu, ctx, StringList(1, hubUrl));
		}
		
		void appendUcMenu(CMenu& menu, int ctx, const StringList& hubs)
		{
			// bool isOp = false; [-] IRainman fix.
			m_userCommands = FavoriteManager::getInstance()->getUserCommands(ctx, hubs/*, isOp*/);
			int n = 0;
			int m = 0;
			
			m_menuPos = menu.GetMenuItemCount();
			
			//if(!m_userCommands.empty() || isOp) // [-]Drakon. Allow op commands for everybody.
			{
				bool l_is_add_responses = ctx != UserCommand::CONTEXT_HUB
				                          &&  ctx != UserCommand::CONTEXT_SEARCH
				                          &&  ctx != UserCommand::CONTEXT_FILELIST;
				if (/*isOp*/l_is_add_responses)
				{
					//[!]IRainman This cycle blocking reproduction operator menu
					for (int i = 0; i < menu.GetMenuItemCount(); i++)
						if (menu.GetMenuItemID(i) == IDC_REPORT)
						{
							l_is_add_responses = false;
							break;
						}
					if (l_is_add_responses)
					{
						menu.AppendMenu(MF_SEPARATOR);
						menu.AppendMenu(MF_STRING, IDC_GET_USER_RESPONSES, CTSTRING(GET_USER_RESPONSES));
						menu.AppendMenu(MF_STRING, IDC_REPORT, CTSTRING(REPORT));
#ifdef IRAINMAN_INCLUDE_USER_CHECK
						menu.AppendMenu(MF_STRING, IDC_CHECKLIST, CTSTRING(CHECK_FILELIST));
#endif
						m_extraItems = 5;
					}
				}
				else
					m_extraItems = 1;
				if (/*isOp*/l_is_add_responses)
					menu.AppendMenu(MF_SEPARATOR);
					
				subMenu.DestroyMenu();
				subMenu.m_hMenu = NULL;
				
				
				if (BOOLSETTING(UC_SUBMENU))
				{
					subMenu.CreatePopupMenu();
					subMenu.InsertSeparatorLast(TSTRING(SETTINGS_USER_COMMANDS));
					
					menu.AppendMenu(MF_POPUP, (HMENU)subMenu, CTSTRING(SETTINGS_USER_COMMANDS));
				}
				
				CMenuHandle cur = BOOLSETTING(UC_SUBMENU) ? subMenu.m_hMenu : menu.m_hMenu;
				
				for (auto ui = m_userCommands.begin(); ui != m_userCommands.end(); ++ui)
				{
					UserCommand& uc = *ui;
					if (uc.getType() == UserCommand::TYPE_SEPARATOR)
					{
						// Avoid double separators...
						if ((cur.GetMenuItemCount() >= 1) &&
						        !(cur.GetMenuState(cur.GetMenuItemCount() - 1, MF_BYPOSITION) & MF_SEPARATOR))
						{
							cur.AppendMenu(MF_SEPARATOR);
							m++;
						}
					}
					
					if (uc.getType() == UserCommand::TYPE_RAW || uc.getType() == UserCommand::TYPE_RAW_ONCE)
					{
						tstring name;
						const auto l_disp_name = uc.getDisplayName();
						cur = BOOLSETTING(UC_SUBMENU) ? subMenu.m_hMenu : menu.m_hMenu;
						for (auto i = l_disp_name.cbegin(); i != l_disp_name.cend(); ++i)
						{
							Text::toT(*i, name);
							if (i + 1 == l_disp_name.cend())
							{
								cur.AppendMenu(MF_STRING, IDC_USER_COMMAND + n, name.c_str());
								m++;
							}
							else
							{
								bool found = false;
								AutoArray<TCHAR> l_buf(1024);
								// Let's see if we find an existing item...
								for (int k = 0; k < cur.GetMenuItemCount(); k++)
								{
									if (cur.GetMenuState(k, MF_BYPOSITION) & MF_POPUP)
									{
										cur.GetMenuString(k, l_buf.data(), 1024, MF_BYPOSITION);
										if (strnicmp(l_buf.data(),  name.c_str(), 1024) == 0)
										{
											found = true;
											cur = (HMENU)cur.GetSubMenu(k);
										}
									}
								}
								if (!found)
								{
									const HMENU l_m = CreatePopupMenu();
									cur.AppendMenu(MF_POPUP, (UINT_PTR)l_m, name.c_str());
									cur = l_m;
								}
							}
						}
					}
					else
					{
						//[-]PPA dcasserta(0);
					}
					n++;
				}
			}
		}
		void cleanUcMenu(OMenu& menu)
		{
			if (!m_userCommands.empty())
			{
				for (size_t i = 0; i < m_userCommands.size() + static_cast<size_t>(m_extraItems); ++i)
				{
					menu.DeleteMenu(m_menuPos, MF_BYPOSITION);
				}
			}
		}
	private:
		UserCommand::List m_userCommands;
		OMenu subMenu;
		int m_menuPos;
		int m_extraItems;
};

#endif // !defined(UC_HANDLER_H)

/**
 * @file
 * $Id: UCHandler.h,v 1.19 2006/07/29 22:51:54 bigmuscle Exp $
 */
