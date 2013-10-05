/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a zlib-style license that can
 *  be found in the License.txt file in the root of the source tree.
 */

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// CriticalSection functions
//
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//---------------------------------------------------------------------------
#ifndef ZenLib_CriticalSectionH
#define ZenLib_CriticalSectionH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#ifdef CS
   #undef CS //Solaris defines this somewhere
#endif
//---------------------------------------------------------------------------

namespace ZenLib
{
#ifdef FLYLINKDC_USE_ZENLIB_CRITICAL_SECTION
//***************************************************************************
/// @brief CriticalSection manipulation
//***************************************************************************

class CriticalSection
{
public :
    //Constructor/Destructor
    CriticalSection  ();
    ~CriticalSection ();

    //Enter/Leave
    void  Enter();
    void  Leave();

private :
    void* CritSect;
};

//***************************************************************************
/// @brief CriticalSectionLocker helper
//***************************************************************************

class CriticalSectionLocker
{
public:
    CriticalSectionLocker (ZenLib::CriticalSection &CS)
    {
        CritSec=&CS;
        CritSec->Enter();
    }

    ~CriticalSectionLocker ()
    {
        CritSec->Leave();
    }

private:
    ZenLib::CriticalSection *CritSec;
};
#else
// [!] FlylinkDC++ Team
class CriticalSection
{
public :
	__forceinline CriticalSection  () {}
	__forceinline ~CriticalSection () {}
	__forceinline void  Enter() {}
	__forceinline void  Leave() {}
};
class CriticalSectionLocker
{
public:
    __forceinline CriticalSectionLocker (const ZenLib::CriticalSection &CS) :CritSec(&CS)
    {
    }
    __forceinline ~CriticalSectionLocker ()
    {
    }
private:
    const ZenLib::CriticalSection *CritSec;
};

#endif
} //NameSpace

#endif
