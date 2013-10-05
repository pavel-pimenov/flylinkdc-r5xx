/*
 * Copyright (C) 2012-2013 FlylinkDC++ Team http://flylinkdc.com/
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

#if !defined(_HTML_COLORS_H_)
#define _HTML_COLORS_H_

#pragma once

struct HTMLColors // [+] SSA struct for BB colors codes
{
	HTMLColors(const tstring& p_tag, const COLORREF p_color) : tag(p_tag), color(p_color) { }
	const tstring tag;
	const COLORREF color;
};

#define RGBT(R,G,B) RGB(0x##R,0x##G,0x##B)

static const HTMLColors g_htmlColors[] =
{
	HTMLColors(_T("red"),       RGBT(ff, 00, 00)),
	HTMLColors(_T("cyan"),      RGBT(00, ff, ff)),
	HTMLColors(_T("blue"),      RGBT(00, 00, ff)),
	HTMLColors(_T("darkblue"),  RGBT(00, 00, a0)),
	HTMLColors(_T("lightblue"), RGBT(ad, d8, e6)),
	HTMLColors(_T("purple"),    RGBT(80, 00, 80)),
	HTMLColors(_T("yellow"),    RGBT(ff, ff, 00)),
	HTMLColors(_T("lime"),      RGBT(00, ff, 00)),
	HTMLColors(_T("fuchsia"),   RGBT(ff, 00, ff)),
	HTMLColors(_T("white"),     RGBT(ff, ff, ff)),
	HTMLColors(_T("silver"),    RGBT(c0, c0, c0)),
	HTMLColors(_T("grey"),      RGBT(80, 80, 80)),
	HTMLColors(_T("black"),     RGBT(00, 00, 00)),
	HTMLColors(_T("orange"),    RGBT(ff, a5, 00)),
	HTMLColors(_T("brown"),     RGBT(a5, 2a, 2a)),
	HTMLColors(_T("maroon"),    RGBT(80, 00, 00)),
	HTMLColors(_T("green"),     RGBT(00, 80, 00)),
	HTMLColors(_T("olive"),     RGBT(80, 80, 00)),
//	HTMLColors(_T(""),          RGBT(00,00,00) ),
};

#undef RGBT

#endif // _HTML_COLORS_H_