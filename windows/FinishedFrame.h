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

#if !defined(FINISHED_FRAME_H)
#define FINISHED_FRAME_H

#pragma once

#include "FinishedFrameBase.h"


class FinishedFrame :
	public FinishedFrameBase<FinishedFrame, ResourceManager::FINISHED_DOWNLOADS, IDC_FINISHED, IDR_FINISHED_DL>
{
	public:
		FinishedFrame(): FinishedFrameBase(e_TransferDownload)
		{
			m_type = FinishedManager::e_Download;
			boldFinished = SettingsManager::BOLD_FINISHED_DOWNLOADS;
			columnOrder = SettingsManager::FINISHED_ORDER;
			columnWidth = SettingsManager::FINISHED_WIDTHS;
			columnVisible = SettingsManager::FINISHED_VISIBLE;
		}
		~FinishedFrame() { }
		
		DECLARE_FRAME_WND_CLASS_EX(_T("FinishedFrame"), IDR_FINISHED_DL, 0, COLOR_3DFACE);
		
	private:
		void on(AddedDl, const FinishedItem* p_entry, bool p_is_sqlite) noexcept
		{
			if (p_is_sqlite)
				SendMessage(WM_SPEAKER, SPEAK_ADD_LINE, (WPARAM)p_entry);
			else
				PostMessage(WM_SPEAKER, SPEAK_ADD_LINE, (WPARAM)p_entry);
		}
		
		void on(RemovedDl, const FinishedItem* p_entry, bool p_is_sqlite) noexcept // [+] IRainman http://code.google.com/p/flylinkdc/issues/detail?id=601
		{
			if (p_is_sqlite)
				SendMessage(WM_SPEAKER, SPEAK_REMOVE_LINE, (WPARAM)p_entry);
			else
				PostMessage(WM_SPEAKER, SPEAK_REMOVE_LINE, (WPARAM)p_entry);
		}
};

#endif // !defined(FINISHED_FRAME_H)

/**
 * @file
 * $Id: FinishedFrame.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
