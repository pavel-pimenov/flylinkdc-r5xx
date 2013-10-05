// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently,
// but are changed infrequently

#pragma once

#include "targetver.h"

// #define _USRDLL // Uncoment this to use GdiOle as dll.

#ifdef _USRDLL
#define IRAINMAN_INCLUDE_GDI_OLE 1
#endif

#ifdef IRAINMAN_INCLUDE_GDI_OLE

#define _ATL_APARTMENT_THREADED
#define _ATL_NO_AUTOMATIC_NAMESPACE

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS  // some CString constructors will be explicit

#include "resource.h"
#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>

using namespace ATL;

#endif // IRAINMAN_INCLUDE_GDI_OLE