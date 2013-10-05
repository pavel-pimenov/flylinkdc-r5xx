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

#if !defined(FAST_ALLOC_H)
#define FAST_ALLOC_H
#pragma once

struct FastAllocBase
{
    static FastCriticalSection cs;
};

 // Fast new/delete replacements for constant sized objects, that also give nice
 // reference locality...
template<class T>
struct FastAlloc : public FastAllocBase
{
        // Custom new & delete that (hopefully) use the node allocator
        static void* operator new(size_t s)
        {
            if (s != sizeof(T))
                return ::operator new(s);
            return allocate();
        }

        // Avoid hiding placement new that's needed by the stl containers...
        static void* operator new(size_t, void* m)
        {
            return m;
        }
        // ...and the warning about missing placement delete...
        static void operator delete(void*, void*)
        {
            // ? We didn't allocate so...
        }

        static void operator delete(void* m, size_t s)
        {
            if (s != sizeof(T))
            {
                ::operator delete(m);
            }
            else if (m != nullptr)
            {
                deallocate((uint8_t*)m);
            }
        }
    protected:
        FastAlloc()
        {
#ifdef PPA_USE_FAST_ALLOC_STATISTICS
            ++g_statist;
#endif
        }
        ~FastAlloc()
        {
#ifdef PPA_USE_FAST_ALLOC_STATISTICS
            --g_statist;
#endif
        }

    private:

        static void* allocate()
        {
            FastLock l(cs);
            if (freeList == NULL)
            {
                grow();
            }
            void* tmp = freeList;
            freeList = *((void**)freeList);
            return tmp;
        }

        static void deallocate(void* p)
        {
            FastLock l(cs);
            *(void**)p = freeList;
            freeList = p;
        }

        static void* freeList;

#ifdef PPA_USE_FAST_ALLOC_STATISTICS
        static CFlyTestStatistic<T> g_statist;
#endif
        static void grow()
        {
            dcassert(sizeof(T) >= sizeof(void*));
            // We want to grow by approximately 128kb at a time...
            // IRainman change allocated size to 1 Mb.
            const size_t items = ((1024 * 1024 + sizeof(T) - 1) / sizeof(T));
            const size_t l_size_alloc = sizeof(T) * items;

#ifdef PPA_USE_FAST_ALLOC_STATISTICS
            g_statist.addAlloc(l_size_alloc);
#endif

#ifdef _WIN32
            freeList = VirtualAlloc(NULL, l_size_alloc, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
            freeList = new uint8_t[l_size_alloc];
#endif
            uint8_t* tmp = (uint8_t*)freeList;
            for (size_t i = 0; i < items - 1; i++)
            {
                *(void**)tmp = tmp + sizeof(T);
                tmp += sizeof(T);
            }
            *(void**)tmp = NULL;
        }
};
template<class T> void* FastAlloc<T>::freeList = NULL;


#ifdef PPA_USE_FAST_ALLOC_STATISTICS
#include "util_flylinkdc.h"
template<class T> struct FastAlloc
{
#ifdef PPA_USE_FAST_ALLOC_STATISTICS
		static CFlyTestStatistic<T> g_statist;
#endif
	public:
		FastAlloc()
		{
#ifdef PPA_USE_FAST_ALLOC_STATISTICS
			++g_statist;
#endif
		}
		~FastAlloc()
		{
#ifdef PPA_USE_FAST_ALLOC_STATISTICS
			--g_statist;
#endif
		}
		static void* operator new(size_t p_size)
		{
#ifdef PPA_USE_FAST_ALLOC_STATISTICS
			g_statist.m_size_alloc += p_size;
			g_statist.m_count_alloc++;
#endif
			return ::operator new(p_size);
		}
		
		static void operator delete(void* p_ptr)
		{
#ifdef PPA_USE_FAST_ALLOC_STATISTICS
			g_statist.m_count_dealloc++;
#endif
			::delete(p_ptr);
		}
		
		static void* operator new[](size_t p_size)
		{
#ifdef PPA_USE_FAST_ALLOC_STATISTICS
			g_statist.m_size_alloc_array += p_size;
			g_statist.m_count_alloc_array++;
#endif
			return ::operator new[](p_size);
		}
		
		static void operator delete[](void* p_ptr)
		{
#ifdef PPA_USE_FAST_ALLOC_STATISTICS
			g_statist.m_count_dealloc_array++;
#endif
			::delete [](p_ptr);
		}
		
};
template<class T> CFlyTestStatistic<T> FastAlloc<T>::g_statist;
#endif

#endif // !defined(FAST_ALLOC_H)

/**
 * @file
 * $Id: FastAlloc.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
