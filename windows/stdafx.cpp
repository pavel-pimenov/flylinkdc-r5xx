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

#include "stdafx.h"
#include "Resource.h"

// Basic sanity checks
#if (_WTL_VER < 0x0810)
#error "WTL not correctly installed, you need version 8.1 or newer!"
#endif

#if _VC80X || _VC90X || _VC10X || _VC11X
#error "You can't compile with Visual C++ Express Editions!"
#endif

#pragma comment(lib, "GdiPlus.lib")

/**
 * @file
 * $Id: stdafx.cpp,v 1.11 2006/05/08 08:36:22 bigmuscle Exp $
 */
