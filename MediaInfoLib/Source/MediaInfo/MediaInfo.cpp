// MediaInfo - All info about media files


//---------------------------------------------------------------------------
// Pre-compilation
#include "MediaInfo/PreComp.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "MediaInfo/Setup.h"
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "MediaInfo/MediaInfo.h"
#include "MediaInfo/MediaInfo_Internal.h"
#if defined(_MSC_VER) && _MSC_VER >= 1800 && _MSC_VER < 1900 && defined(_M_X64)
    #include <math.h> // needed for _set_FMA3_enable()
#endif
using namespace ZenLib;
//---------------------------------------------------------------------------

namespace MediaInfoLib
{

//---------------------------------------------------------------------------
//To clarify the code
namespace MediaInfo_Debug_MediaInfo
{

#if defined (MEDIAINFO_DEBUG_CONFIG) || defined (MEDIAINFO_DEBUG_BUFFER) || defined (MEDIAINFO_DEBUG_OUTPUT)
    #ifdef WINDOWS
        const Char* MediaInfo_Debug_Name=__T("MediaInfo_Debug");
    #else
        const Char* MediaInfo_Debug_Name=__T("/tmp/MediaInfo_Debug");
    #endif
#endif

#ifdef MEDIAINFO_DEBUG_CONFIG
    #define MEDIAINFO_DEBUG_STATIC(_TOAPPEND) \
        { \
            File F(Ztring(MediaInfo_Debug_Name)+__T(".Config.static.txt"), File::Access_Write_Append); \
            Ztring Debug; \
            _TOAPPEND; \
            Debug+=__T("\r\n"); \
            F.Write(Debug); \
            F.Close(); \
        }
#else // MEDIAINFO_DEBUG_CONFIG
    #define MEDIAINFO_DEBUG_STATIC(_TOAPPEND)
#endif // MEDIAINFO_DEBUG_CONFIG

}
using namespace MediaInfo_Debug_MediaInfo;

//***************************************************************************
// Constructor/destructor
//***************************************************************************

//---------------------------------------------------------------------------
MediaInfo::MediaInfo()
{
    Internal=new MediaInfo_Internal();

    // FMA3 support in the 2013 CRT is broken on Vista and Windows 7 RTM (fixed in SP1).
    // See https://connect.microsoft.com/VisualStudio/feedback/details/987093/x64-log-function-uses-vpsrlq-avx-instruction-without-regard-to-operating-system-so-it-crashes-on-vista-x64
    // Hotfix: we disable it for MSVC2013.
    #if defined(_MSC_VER) && _MSC_VER >= 1800 && _MSC_VER < 1900 && defined(_M_X64)
        _set_FMA3_enable(0);
    #endif
}

//---------------------------------------------------------------------------
MediaInfo::~MediaInfo()
{
    delete Internal; //Internal=NULL;
}

//***************************************************************************
// Files
//***************************************************************************

//---------------------------------------------------------------------------
size_t MediaInfo::Open(const String &File_Name_)
{
#if defined(MEDIAINFO_FILE_YES)
    return Internal->Open(File_Name_);
#else //defined(MEDIAINFO_FILE_YES)
    return 0; //Not available
#endif //defined(MEDIAINFO_FILE_YES)
}

//---------------------------------------------------------------------------
size_t MediaInfo::Open (const int8u* Begin, size_t Begin_Size, const int8u* End, size_t End_Size, int64u File_Size)
{
    return Internal->Open(Begin, Begin_Size, End, End_Size, (File_Size<Begin_Size+End_Size)?(Begin_Size+End_Size):File_Size);
}

//---------------------------------------------------------------------------
size_t MediaInfo::Open_Buffer_Init (int64u File_Size, int64u File_Offset)
{
    return Internal->Open_Buffer_Init(File_Size, File_Offset);
}

//---------------------------------------------------------------------------
size_t MediaInfo::Open_Buffer_Continue (const int8u* ToAdd, size_t ToAdd_Size)
{
    return Internal->Open_Buffer_Continue(ToAdd, ToAdd_Size).to_ulong();
}

//---------------------------------------------------------------------------
int64u MediaInfo::Open_Buffer_Continue_GoTo_Get ()
{
    return Internal->Open_Buffer_Continue_GoTo_Get();
}

//---------------------------------------------------------------------------
size_t MediaInfo::Open_Buffer_Finalize ()
{
    return Internal->Open_Buffer_Finalize();
}

//---------------------------------------------------------------------------
size_t MediaInfo::Open_NextPacket ()
{
    return Internal->Open_NextPacket().to_ulong();
}

//---------------------------------------------------------------------------
size_t MediaInfo::Save()
{
    return 0; //Not yet implemented
}

//---------------------------------------------------------------------------
void MediaInfo::Close()
{
    return Internal->Close();
}

//***************************************************************************
// Get File info
//***************************************************************************

//---------------------------------------------------------------------------
#ifdef FLYLINKDC_USE_MEDIAINFO_INFORM
String MediaInfo::Inform(size_t)
{
    return MediaInfo_Internal::Inform(Internal);
}
#endif
//---------------------------------------------------------------------------
String MediaInfo::Get(stream_t StreamKind, size_t StreamPos, size_t Parameter, info_t KindOfInfo)
{
    return Internal->Get(StreamKind, StreamPos, Parameter, KindOfInfo);
}

//---------------------------------------------------------------------------
String MediaInfo::Get(stream_t StreamKind, size_t StreamPos, const String &Parameter, info_t KindOfInfo, info_t KindOfSearch)
{
    return Internal->Get(StreamKind, StreamPos, Parameter, KindOfInfo, KindOfSearch);
}

//***************************************************************************
// Set File info
//***************************************************************************

//---------------------------------------------------------------------------
size_t MediaInfo::Set(const String &, stream_t, size_t, size_t, const String &)
{
    return 0; //Not yet implemented
}

//---------------------------------------------------------------------------
size_t MediaInfo::Set(const String &, stream_t, size_t, const String &, const String &)
{
    return 0; //Not yet implemented
}

//***************************************************************************
// Output buffer
//***************************************************************************

//---------------------------------------------------------------------------
size_t MediaInfo::Output_Buffer_Get (const String &Value)
{
    return Internal->Output_Buffer_Get(Value);
}

//---------------------------------------------------------------------------
size_t MediaInfo::Output_Buffer_Get (size_t Pos)
{
    return Internal->Output_Buffer_Get(Pos);
}

//***************************************************************************
// Information
//***************************************************************************

//---------------------------------------------------------------------------
String MediaInfo::Option (const String &Option, const String &Value)
{
    return Internal->Option(Option, Value);
}

//---------------------------------------------------------------------------
String MediaInfo::Option_Static (const String &Option, const String &Value)
{
    MEDIAINFO_DEBUG_STATIC(Debug+=__T("Option_Static, Option=");Debug+=Ztring(Option);Debug+=__T(", Value=");Debug+=Ztring(Value);)
    MediaInfoLib::Config.Init(); //Initialize Configuration

    if (Option==__T("Info_Capacities"))
    {
        return __T("Option disactivated for this version, will come back soon!");
        //MediaInfo_Internal MI;
        //return MI.Option(Option);
    }
    else if (Option==__T("Info_Version"))
    {
        Ztring ToReturn=MediaInfoLib::Config.Info_Version_Get();
        if (MediaInfo_Internal::LibraryIsModified())
            ToReturn+=__T(" modified");
        return ToReturn;
    }
    else
        return MediaInfoLib::Config.Option(Option, Value);
}

//---------------------------------------------------------------------------
size_t MediaInfo::Count_Get (stream_t StreamKind, size_t StreamPos)
{
    return Internal->Count_Get(StreamKind, StreamPos);

}

//---------------------------------------------------------------------------
size_t MediaInfo::State_Get ()
{
    return Internal->State_Get();
}

} //NameSpace
