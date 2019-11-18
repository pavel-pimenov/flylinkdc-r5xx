/*
 * Copyright (C) 2003-2013 P�r Bj�rklund, per.bjorklund@gmail.com
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

#ifndef POPUPMANAGER_H
#define POPUPMANAGER_H

#pragma once

#include "../client/Singleton.h"
#include "../client/TimerManager.h"
#include "PopupDlg.h"

#define DOWNLOAD_COMPLETE 6

class PopupManager : public Singleton< PopupManager >, private TimerManagerListener
{
	public:
		PopupManager() : height(90), width(200), offset(0), m_is_activated(true), m_id(0), m_popuptype(0), m_hBitmap(0)
		{
			TimerManager::getInstance()->addListener(this);
		}
		
		~PopupManager()
		{
			dcassert(m_popups.empty());
			TimerManager::getInstance()->removeListener(this);
			if (m_hBitmap)
			{
				::DeleteObject(m_hBitmap);
			}
		}
		
		enum { BALLOON, CUSTOM, SPLASH, WINDOW };
		
		//call this with a preformatted message
		void Show(const tstring &aMsg, const tstring &aTitle, int Icon, bool preview = false);
		
		//remove first popup in list and move everyone else
		void Remove(uint32_t pos = 0);
		
		//remove the popups that are scheduled to be removed
		void AutoRemove(uint64_t tick);
		
		void Mute(bool mute)
		{
			m_is_activated = !mute;
		}
		
	private:
		typedef deque< PopupWnd* > PopupList;
		PopupList m_popups;
		
		//size of the popup window
		uint16_t height;
		uint16_t width;
		
		//if we have multiple windows displayed,
		//keep track of where the new one will be displayed
		uint16_t offset;
		int m_popuptype;
		
		//id of the popup to keep track of them
		uint32_t m_id;
		
		//turn on/off popups completely
		bool m_is_activated;
		
		//for custom popups
		HBITMAP m_hBitmap;
		string PopupImage;
		
		// TimerManagerListener
		void on(TimerManagerListener::Second, uint64_t tick) noexcept override;
		
};

#endif
