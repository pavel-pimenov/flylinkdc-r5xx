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

#if !defined(BIT_OUTPUT_STREAM_H)
#define BIT_OUTPUT_STREAM_H

class BitOutputStream
#ifdef _DEBUG
	: boost::noncopyable
#endif
{
	public:
		explicit BitOutputStream(string& aStream) : is(aStream), bitPos(0), next(0) { }
		~BitOutputStream() { }
		
		void put(const ByteVector& b)
		{
			for (auto i = b.cbegin(); i != b.cend(); ++i)
			{
				next |= (*i) << bitPos++;
				
				if (bitPos > 7)
				{
					bitPos -= 8;
					is += next;
					next = 0;
				}
				
			}
		}
		
		void skipToByte()
		{
			if (bitPos > 0)
			{
				bitPos = 0;
				is += next;
				next = 0;
			}
		}
		
	private:
		string& is;
		int bitPos;
		uint8_t next;
};

#endif // !defined(BIT_OUTPUT_STREAM_H)

/**
 * @file
 * $Id: BitOutputStream.h,v 1.11 2006/10/13 20:04:31 bigmuscle Exp $
 */
