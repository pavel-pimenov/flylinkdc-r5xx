/*
  * Copyright (c) 2013 Developed by reg <entry.reg@gmail.com>
  *
  * Distributed under the Boost Software License, Version 1.0
  * (see accompanying file LICENSE or a copy at http://www.boost.org/LICENSE_1_0.txt)
 */
// revision sha1: 2bc1073f9953

#include "stdinc.h"
#include "WildcardsReg.h"

namespace net
{
namespace r_eg
{
namespace text
{
namespace wildcards
{

/**
 * entry for match cases
 * TODO:
 *  - implement: ONE, MORE, SINGLE for BEGIN & any MS comb. for END [current return value is FALSE]
 *  - MS combination: (item.delta == 0) -> unsigned short int bitmask & pre-comb. variations
 */
bool WildcardsEss::search(const tstring& text, const tstring& filter, bool ignoreCase)
{
	if (filter.empty())
	{
		return true;
	}
	
	if (ignoreCase)
	{
		//TODO: [perfomance] by single char for iterator
		_text   = _lowercase(text);
		_filter = _lowercase(filter); //: ~18ms
	}
	
	item.reset();
	words.reset();
	
	for (tstring::const_iterator it = _filter.begin(), itEnd = _filter.end(); it != itEnd; ++it)
	{
		++item.left;
		
		switch (*it)
		{
			case MS_ANY:
			{
				item.mask.curr = ANY;
				break;
			}
			case MS_ANYSP:
			{
				item.mask.curr = ANYSP;
				break;
			}
			case MS_SPLIT:
			{
				item.mask.curr = SPLIT;
				break;
			}
			case MS_ONE:
			{
				item.mask.curr = ONE;
				break;
			}
			case MS_BEGIN:
			{
				item.mask.curr = BEGIN;
				break;
			}
			case MS_END:
			{
				item.mask.curr = END;
				break;
			}
			case MS_MORE:
			{
				item.mask.curr = MORE;
				break;
			}
			case MS_SINGLE:
			{
				item.mask.curr = SINGLE;
				break;
			}
			default:
			{
				if (it + 1 == itEnd)
				{
					item.mask.curr = EOL;
					++item.left;
				}
				else
				{
					continue;
				}
			}
		}
		
		/* When previous symbol is a meta-symbol - delta equal 0 */
		
		if ((item.delta = item.left - 1 - item.pos) == 0)
		{
			if (item.mask.curr & (SPLIT | EOL))
			{
				return true;
			}
			
			if (item.mask.curr & BEGIN && (item.mask.prev & (BOL | SPLIT)) == 0) // is not: BOL^__ or SPLIT^__
			{
				if (rewindToNextBlock(it))
				{
					continue;
				}
				return false;
			}
			else if (item.mask.curr & END) // combination found, e.g.: *$, ??$, etc. TODO: stub - _stubENDCombination()
			{
				if (rewindToNextBlock(it))
				{
					continue;
				}
				return false;
			}
			
			// Sequential combinations of characters SINGLE & ONE
			if ((item.mask.curr & SINGLE && item.mask.prev & SINGLE) ||
			        (item.mask.curr & ONE && item.mask.prev & ONE))
			{
				++item.overlay;
			}
			else
			{
				item.overlay = 0;
			}
			
			// disable all combinations for SINGLE. TODO: stub - _stubSINGLECombination()
			if ((item.mask.prev & (BOL | EOL)) == 0 &&
			        (
			            (item.mask.curr & SINGLE && (item.mask.prev & SINGLE) == 0) ||
			            (item.mask.prev & SINGLE && (item.mask.curr & SINGLE) == 0)))
			{
				if (rewindToNextBlock(it))
				{
					continue;
				}
				return false;
			}
			
			++item.pos;
			
			// TODO: implement non-sequential MS combinations ~ unsigned short int ..
			if ((item.mask.prev & ANYSP) == 0)
			{
				item.mask.prev = item.mask.curr;
			}
			continue;
		}
		
		/* Otherwise work with a part of word ... */
		
		if (item.mask.curr & BEGIN) // __^xxx
		{
			if (rewindToNextBlock(it))
			{
				continue;
			}
			return false;
		}
		
		// getting word item
		item.curr = _filter.substr(item.pos, item.delta);
		
		if (item.mask.curr & END)
		{
			if (item.delta >= _text.length() && _text.substr(_text.length() - item.delta).compare(item.curr) == 0)
			{
				if (it + 1 == itEnd)
				{
					return true; /*EOL*/
				}
				
				switch (*(it + 1))
				{
					case MS_SPLIT:
					{
						return true;
					}
				}
			}
			// __$x
			if (rewindToNextBlock(it))
			{
				continue;
			}
			return false;
		}
		
		if (item.mask.prev & BEGIN)
		{
			if (_text.substr(0, item.delta).compare(item.curr) == 0)
			{
				if (item.mask.curr & (SPLIT | EOL))
				{
					return true;
				}
				words.found = 0;
			}
			else
			{
				if (item.mask.curr & EOL)
				{
					return false;
				}
				if (item.mask.curr & SPLIT)
				{
					continue;
				}
				if (rewindToNextBlock(it))
				{
					continue;
				}
				return false;
			}
		}
		else
		{
			// find a part
			if (item.mask.prev & MORE)
			{
				++words.left;
			}
			words.found = _text.find(item.curr, words.left);
		}
		
		// working with an interval
		if (words.found != tstring::npos)
		{
			words.found = interval();
		}
		item.overlay = 0; //flush sequence
		
		/* SPLIT control */
		
		if (words.found == tstring::npos)
		{
			if (item.mask.curr & EOL) //TODO: [optimize]: ...or last split-block
			{
				return false;
			}
			
			item.pos = item.left;
			if (item.mask.curr & SPLIT)
			{
				continue; //to next block
			}
			if (rewindToNextBlock(it, false))
			{
				continue;
			}
			return false;
		}
		
		/* Success: */
		
		if (item.mask.curr & (SPLIT | EOL))
		{
			return true;
		}
		
		item.pos        = item.left;
		words.left      = words.found + item.delta;
		item.mask.prev  = item.mask.curr;
		item.prev       = item.curr;
	}
	
	/* after ending iteration: */
	
	if (item.mask.prev & MORE && words.left > 0) // e.g: {word}EOL + {1,~}
	{
		return false;
	}
	
	if (item.mask.prev & SINGLE)
	{
		if (words.left == _text.length()) // e.g: {word}EOL + {1}
		{
			return false;
		}
	}
	
	if (item.mask.prev & ANYSP) // {word}>***??* etc.
	{
		if (_text.substr(words.left).find(ANYSP_CMP_DEFAULT) != tstring::npos)
		{
			return false;
		}
	}
	return true;
}

size_t WildcardsEss::interval()
{
	// "#"
	if (item.mask.prev & SINGLE && (words.found - words.left) != 1)
	{
		size_t len      = item.prev.length();
		short int lPos  = words.found - len - item.overlay - 1;
		
		// [pro]ject ... [pro]t[ection] -> [pro]<-#-ection
		if (lPos < 0 || _text.substr(lPos, len).compare(item.prev) != 0)
		{
			return tstring::npos;
		}
		return words.found;
	}
	
	// "?"
	if (item.mask.prev & ONE && (words.found - words.left) > 1)
	{
		size_t len      = item.prev.length();
		size_t lPosMax  = words.found - len;
		size_t lPos     = lPosMax - item.overlay - 1;
		
		do  // ????? - min<->max:
		{
			if (_text.substr(lPos, len).compare(item.prev) == 0)
			{
				return words.found;
			}
		}
		while (++lPos < lPosMax);
		
		return tstring::npos;
	}
	
	// ">"
	if (item.mask.prev & ANYSP)
	{
		tstring inside = _text.substr(words.left, words.found - words.left);
		if (inside.find(ANYSP_CMP_DEFAULT) != tstring::npos)
		{
			return tstring::npos;
		}
		return words.found;
	}
	
	//....
	return words.found;
}

inline bool WildcardsEss::rewindToNextBlock(tstring::const_iterator& it, bool delta)
{
	item.left = _filter.find(MS_SPLIT, item.left);
	if (item.left == tstring::npos)
	{
		return false; //EOL
	}
	
	if (delta)
	{
		it += ++item.left - (item.pos + item.delta) - 1; //or overload =
	}
	else
	{
		it += ++item.left - item.pos;
	}
	
	item.pos        = item.left;
	words.left      = 0;
	item.mask.prev  = SPLIT;
	return true;
}

}
}
}
}
