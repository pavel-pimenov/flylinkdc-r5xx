/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a zlib-style license that can
 *  be found in the License.txt file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef ZenOS_UtilsH
#define ZenOS_UtilsH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "ZenLib/Ztring.h"
#ifdef WINDOWS
#ifndef ZENLIB_NO_WIN9X_SUPPORT
    #undef __TEXT
    #include "windows.h"
#endif //ZENLIB_NO_WIN9X_SUPPORT
#endif //WINDOWS
//---------------------------------------------------------------------------

namespace ZenLib
{

#ifdef WINDOWS
#ifndef ZENLIB_NO_WIN9X_SUPPORT
//***************************************************************************
// OS Information
//***************************************************************************

//---------------------------------------------------------------------------
// [!] FlylinkDC opt: We will help the optimizer to throw out the check and the code of it depends on your code: add "inline const static" and move to header to garantee inline implementation on this function.
inline static const bool IsWin9X  ()
{
    #ifdef ZENLIB_USEWX
        return true;
    #else //ZENLIB_USEWX
        #ifdef WINDOWS
			#ifdef ZENLIB_USE_IN_WIN98
	            if (GetVersion() >= 0x80000000)
		            return true;
			    else
			#endif //ZENLIB_USE_IN_WIN98
				    return false;
        #else //WINDOWS
            return true;
        #endif //WINDOWS
    #endif //ZENLIB_USEWX
}
inline bool IsWin9X_Fast ()
{
    return GetVersion()>=0x80000000;
}
#endif //ZENLIB_NO_WIN9X_SUPPORT
#endif //WINDOWS

//***************************************************************************
// Execute
//***************************************************************************

void Shell_Execute(const Ztring &ToExecute);

//***************************************************************************
// Directorues
//***************************************************************************

Ztring OpenFolder_Show(void* Handle, const Ztring &Title, const Ztring &Caption);

} //namespace ZenLib
#endif
