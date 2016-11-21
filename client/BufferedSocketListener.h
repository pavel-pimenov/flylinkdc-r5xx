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

#pragma once


#ifndef DCPLUSPLUS_DCPP_BUFFEREDSOCKETLISTENER_H_
#define DCPLUSPLUS_DCPP_BUFFEREDSOCKETLISTENER_H_

#include "noexcept.h"
#include "typedefs.h"
#include "CFlySearchItemTTH.h"

class BufferedSocketListener
{
	public:
		virtual ~BufferedSocketListener() { }
		template<int I> struct X
		{
			enum { TYPE = I };
		};
		
		typedef X<0> Connecting;
		typedef X<1> Connected;
		typedef X<2> Line;
		typedef X<3> Data;
		//typedef X<4> BytesSent;
#ifdef FLYLINKDC_USE_CROOKED_HTTP_CONNECTION
		typedef X<5> ModeChange;
#endif
		typedef X<6> TransmitDone;
		typedef X<7> Failed;
		typedef X<8> Updated;
		typedef X<9> MyInfoArray;
		typedef X<10> SearchArrayTTH;
		typedef X<11> SearchArrayFile;
		typedef X<12> DDoSSearchDetect;
		
		virtual void on(Connecting) noexcept { }
		virtual void on(Connected) noexcept { }
		virtual void on(Line, const string&) noexcept { }
		virtual void on(MyInfoArray, StringList&) noexcept { }
		virtual void on(DDoSSearchDetect, const string&) noexcept { }
		virtual void on(SearchArrayTTH, CFlySearchArrayTTH&) noexcept { }
		virtual void on(SearchArrayFile, const CFlySearchArrayFile&) noexcept { }
		//virtual void on(Data, uint8_t*, size_t) noexcept { }
		//virtual void on(BytesSent, size_t p_Bytes, size_t p_Actual) noexcept { }
#ifdef FLYLINKDC_USE_CROOKED_HTTP_CONNECTION
		virtual void on(ModeChange) noexcept {}
#endif
		virtual void on(TransmitDone) noexcept { }
		virtual void on(Failed, const string&) noexcept { }
		virtual void on(Updated) noexcept { }
};

#endif /*BUFFEREDSOCKETLISTENER_H_*/
