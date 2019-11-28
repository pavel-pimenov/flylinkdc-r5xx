/*
 * Copyright (C) 2001-2017 Jacek Sieka, j_s@telia.com
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
#include "Exception.h"


Exception::Exception(const string& aError) : m_error(aError)
{
	dcdebug("Thrown: %s\n", m_error.c_str());
#ifdef _DEBUG
	std::ofstream l_fs;
	l_fs.open(L"flylinkdc-Exception-error.log", std::ifstream::out | std::ifstream::app);
	if (l_fs.good())
	{
		l_fs << " m_error = " << m_error << std::endl;
	}
	else
	{
		dcassert(0);
	}
#endif
}

/**
 * @file
 * $Id: Exception.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
