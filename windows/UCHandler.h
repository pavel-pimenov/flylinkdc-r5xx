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

#include "../client/FavoriteManager.h"
#include "../client/ClientManager.h"
#include "OMenu.h"

template<class T>
class UCHandler
{
	public:
		UCHandler() : menuPos(0), extraItems(0) { }
		
		typedef UCHandler<T> thisClass;
		BEGIN_MSG_MAP(thisClass)
		COMMAND_RANGE_HANDLER(IDC_USER_COMMAND, IDC_USER_COMMAND + userCommands.size(), onUserCommand)
		END_MSG_MAP()
		
		LRESULT onUserCommand(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			dcassert(wID >= IDC_USER_COMMAND);
			size_t n = (size_t)wID - IDC_USER_COMMAND;
			dcassert(n < userCommands.size());
			
			UserCommand& uc = userCommands[n];
			
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
			userCommands = FavoriteManager::getInstance()->getUserCommands(ctx, hubs/*, isOp*/);
			int n = 0;
			int m = 0;
			
			menuPos = menu.GetMenuItemCount();
			
			//if(!userCommands.empty() || isOp) // [-]Drakon. Allow op commands for everybody.
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
//#ifdef OLD_MENU_HEADER //[~]JhaoDa
						menu.AppendMenu(MF_SEPARATOR);
//#endif
						menu.AppendMenu(MF_STRING, IDC_GET_USER_RESPONSES, CTSTRING(GET_USER_RESPONSES));
						menu.AppendMenu(MF_STRING, IDC_REPORT, CTSTRING(REPORT));
						menu.AppendMenu(MF_STRING, IDC_CHECKLIST, CTSTRING(CHECK_FILELIST));
						extraItems = 5;
					}
				}
				else
					extraItems = 1;
				if (/*isOp*/l_is_add_responses)
					menu.AppendMenu(MF_SEPARATOR);
					
				CMenuHandle cur = menu.m_hMenu;
				for (auto ui = userCommands.begin(); ui != userCommands.end(); ++ui)
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
						cur = menu.m_hMenu;
						for (auto i = uc.getDisplayName().cbegin(), iend = uc.getDisplayName().cend(); i != iend; ++i)
						{
							Text::toT(*i, name);
							if (i + 1 == iend)
							{
								cur.AppendMenu(MF_STRING, IDC_USER_COMMAND + n, name.c_str());
								m++;
							}
							else
							{
								bool found = false;
								LocalArray<TCHAR, 1024> buf;
								// Let's see if we find an existing item...
								for (int k = 0; k < cur.GetMenuItemCount(); k++)
								{
									if (cur.GetMenuState(k, MF_BYPOSITION) & MF_POPUP)
									{
										cur.GetMenuString(k, buf.data(), 1024, MF_BYPOSITION);
										if (stricmp(buf.data(),  name.c_str()) == 0)
										{
											found = true;
											cur = (HMENU)cur.GetSubMenu(k);
										}
									}
								}
								if (!found)
								{
									HMENU l_m = CreatePopupMenu();
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
			if (!userCommands.empty())
			{
				for (size_t i = 0; i < userCommands.size() + static_cast<size_t>(extraItems); ++i)
				{
					menu.DeleteMenu(menuPos, MF_BYPOSITION);
				}
			}
		}
	private:
		UserCommand::List userCommands;
		int menuPos;
		int extraItems;
};

#endif // !defined(UC_HANDLER_H)

/**
 * @file
 * $Id: UCHandler.h,v 1.19 2006/07/29 22:51:54 bigmuscle Exp $
 */
