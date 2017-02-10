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


#if !defined(DCPLUSPLUS_WIN32_STDAFX_H)
#define DCPLUSPLUS_WIN32_STDAFX_H


#pragma once

#include "../client/stdinc.h"
#include "../client/ResourceManager.h"

#ifdef _WIN32

#define _WTL_NO_CSTRING

#include <atlbase.h>
#include <atlapp.h>

extern CAppModule _Module;

#define _WTL_MDIWINDOWMENU_TEXT CTSTRING(MENU_WINDOW)
#ifdef OLD_MENU_HEADER
#define _WTL_CMDBAR_VISTA_MENUS 0 // Don't turned on :( http://www.youtube.com/watch?v=2b8GcBkjdb8
#else
#define _WTL_CMDBAR_VISTA_MENUS 1
#endif
#define _WTL_NEW_PAGE_NOTIFY_HANDLERS // [+] SSA For Wizard
//#define _WTL_NO_AUTO_THEME 1
#include <atlwin.h>
#include <atlframe.h>
#include <atlctrls.h>
#include <atldlgs.h>
#include <atlctrlw.h>
#include <atlmisc.h>
#include <atlsplit.h>
#include <atltheme.h>
#endif // _WIN32

#endif // DCPLUSPLUS_WIN32_STDAFX_H

/**
 * @file
 * $Id: stdafx.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
