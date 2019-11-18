// Copyright (C) 1996 - 2002 Florian Schintke
// Modified 2002 by Opera, opera@home.se
//
// This is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2, or (at your option) any later
// version.
//
// Thanks to the E.S.O. - ACS project that has done this C++ interface
// to the wildcards pttern matching algorithm


#pragma once

#ifndef WILDCARD_H
#define WILDCARD_H

#include "StringTokenizer.h"
#include "Util.h"

// Implementation of the UN*X wildcards
// Supported wild-characters: '*', '?'; sets: [a-z], '!' negation
// Examples:
//       '[a-g]l*i?n' matches 'florian'
//       '[!abc]*e' matches 'smile'
//       '[-z] matches 'a'

class Wildcard
{
	public:
		// This function implements the UN*X wildcards and returns:
		// 0 - if *wildcard does not match *test
		// 1 - if *wildcard matches *test
		template <typename C>
		static int wildcardfit(const C *wildcard, const C *test, const bool useSet = true)
		{
			int fit = 1;
			
			for (; ('\000' != *wildcard) && (1 == fit) && ('\000' != *test); wildcard++)
			{
				switch (*wildcard)
				{
					case '?':
						test++;
						break;
					case '*':
						fit = asterisk(&wildcard, &test);
						// the asterisk was skipped by asterisk() but the loop will
						// increment by itself. So we have to decrement
						wildcard--;
						break;
					case '[':
						if (useSet)
						{
							wildcard++; // leave out the opening square bracket
							fit = set(&wildcard, &test);
							// we don't need to decrement the wildcard as in case
							// of asterisk because the closing ] is still there
							break;
						} //if we're not using the set option, fall through
					default:
						fit = (int)(*wildcard == *test);
						test++;
				}
			}
			while ((*wildcard == '*') && (1 == fit))
				// here the teststring is empty otherwise you cannot
				// leave the previous loop
				wildcard++;
			return (int)((1 == fit) && ('\0' == *test) && ('\0' == *wildcard));
		}
		
		// Checks whether a text matches a pattern
		template<class STR>
		static bool patternMatchLowerCase(const STR& text, const STR& pattern, const bool useSet = true)
		{
			return wildcardfit(pattern.c_str(), text.c_str(), useSet) == 1;
		}
		// Checks whether a text matches any pattern in a patternlist
		template<class STR, class LIST>
		static bool patternMatchLowerCase(const STR& text, const LIST& patternlist, const bool useSet = true)
		{
			for (auto i = patternlist.cbegin(); i != patternlist.cend(); ++i)
			{
				if (patternMatchLowerCase(text, *i, useSet))
				{
					return true;
				}
			}
			return false;
		}
		template<class STR>
		static bool patternMatchLowerCase(const STR& text, const STR& patternlist, const typename STR::value_type delimiter, const bool useSet = true)
		{
			dcassert(!text.empty());
			dcassert(!patternlist.empty());
			const StringTokenizer<STR> tokens(patternlist, delimiter);
			return patternMatchLowerCase(text, tokens.getTokens(), useSet);
		}
		template<class STR>
		static bool patternMatch(const STR& text, const STR& patternlist, const typename STR::value_type delimiter, const bool useSet = true)
		{
			dcassert(!text.empty());
			dcassert(!patternlist.empty());
			return patternMatchLowerCase(Text::toLower(text), Text::toLower(patternlist), delimiter, useSet);
		}
		
	private:
		// Scans a set of characters and returns 0 if the set mismatches at this
		// position in the teststring and 1 if it is matching
		// wildcard is set to the closing ] and test is unmodified if mismatched
		// and otherwise the char pointer is pointing to the next character
		template <typename C>
		static int set(const C **wildcard, const C **test)
		{
			int fit = 0;
			int negation = 0;
			int at_beginning = 1;
			
			if ('!' == **wildcard)
			{
				negation = 1;
				(*wildcard)++;
			}
			while ((**wildcard) != '\0' &&
			        ((']' != **wildcard) || (1 == at_beginning)))
			{
				if (0 == fit)
				{
					if (('-' == **wildcard)
					        && ((*(*wildcard - 1)) < (*(*wildcard + 1)))
					        && (']' != *(*wildcard + 1))
					        && (0 == at_beginning))
					{
						if (((**test) >= (*(*wildcard - 1)))
						        && ((**test) <= (*(*wildcard + 1))))
						{
							fit = 1;
							(*wildcard)++;
						}
					}
					else if ((**wildcard) == (**test))
					{
						fit = 1;
					}
				}
				(*wildcard)++;
				at_beginning = 0;
			}
			if (1 == negation)
				// change from zero to one and vice versa
				fit = 1 - fit;
			if (1 == fit)
				(*test)++;
				
			return fit;
		}
		
		// Scans an asterisk
		template <typename C>
		static int asterisk(const C **wildcard, const C **test)
		{
			int fit = 1;
			
			// erase the leading asterisk
			(*wildcard)++;
			while (('\000' != (**test))
			        && (('?' == **wildcard)
			            || ('*' == **wildcard)))
			{
				if ('?' == **wildcard)
					(*test)++;
				(*wildcard)++;
			}
			// Now it could be that test is empty and wildcard contains
			// aterisks. Then we delete them to get a proper state
			while ('*' == (**wildcard))
				(*wildcard)++;
				
			if (('\0' == (**test)) && ('\0' != (**wildcard)))
			{
				fit = 0;
			}
			else if (('\0' == (**test)) && ('\0' == (**wildcard)))
			{
				fit = 1;
			}
			else
			{
				// Neither test nor wildcard are empty!
				// the first character of wildcard isn't in [*?]
				if (0 == wildcardfit(*wildcard, (*test)))
				{
					do
					{
						(*test)++;
						// skip as much characters as possible in the teststring
						// stop if a character match occurs
						while (((**wildcard) != (**test))
						        && ('['  != (**wildcard))
						        && ('\0' != (**test)))
							(*test)++;
					}
					while ((('\0' != **test)) ?
					        (0 == wildcardfit(*wildcard, (*test)))
					        : (0 != (fit = 0)));
				}
				if (('\0' == **test) && ('\0' == **wildcard))
				{
					fit = 1;
				}
			}
			return fit;
		}
};

#endif
