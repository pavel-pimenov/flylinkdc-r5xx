/*
 * Copyright (C) 2006-2013 Crise, crise<at>mail.berlios.de
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

#ifndef RESOURCE_LOADER_H
#define RESOURCE_LOADER_H

#pragma once

#ifdef __ATLMISC_H__
# define __ATLTYPES_H__
#endif

# include "../client/Singleton.h"
# include "../client/Pointer.h"

#include <atlimage.h>

class ExCImage : public CImage
{
	public:
		ExCImage(): m_hBuffer(nullptr)
		{
		}
		explicit ExCImage(LPCTSTR pszFileName);
		ExCImage(UINT id, LPCTSTR pType = RT_RCDATA);
		ExCImage(UINT id, UINT type);
		~ExCImage()
		{
			Destroy();
		}
		bool LoadFromResourcePNG(UINT id);
		bool LoadFromResource(UINT id, LPCTSTR pType = RT_RCDATA);
		void Destroy() noexcept;
		
	private:
		HGLOBAL m_hBuffer;
};

class ResourceLoader
{
	public:
		static int LoadImageList(LPCTSTR pszFileName, CImageList& aImgLst, int cx, int cy);
		static int LoadImageList(UINT id, CImageList& aImgLst, int cx, int cy);
};

#endif // RESOURCE_LOADER_H