/*
 * Copyright (C) 2011-2013 AirDC++ Project
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


#include "stdafx.h"
#include "Players.h"
#include "../client/Text.h"
#include "../client/SettingsManager.h"
#include "../client/File.h"
#include "MainFrm.h"

#include "iTunesCOMInterface.h"
#include "WMPlayerRemoteApi.h"
#include "winamp.h"
#include "QCDCtrlMsgs.h"

#include <atlwin.h>
#include <atlcomtime.h>
#include <control.h>
#include <strmif.h> // error with missing ddraw.h, get it from MS DirectX SDK

#include "boost/algorithm/string/replace.hpp"

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <cstring>


// This is GPLed, and copyright (mostly) my anonymous friend
// - Todd Pederzani
string Players::getItunesSpam(HWND playerWnd /*= NULL*/)
{
	// If it's not running don't even bother...
	if (playerWnd != NULL)
	{
		// Pointer
		IiTunes *iITunes;
		
		// Load COM library
		CoInitialize(NULL);
		
		// Others
		StringMap params;
		
		// note - CLSID_iTunesApp and IID_IiTunes are defined in iTunesCOMInterface_i.c
		//Create an instance of the top-level object.  iITunes is an interface pointer to IiTunes.  (weird capitalization, but that's how Apple did it)
		if (SUCCEEDED(::CoCreateInstance(CLSID_iTunesApp, NULL, CLSCTX_LOCAL_SERVER, IID_IiTunes, (PVOID *)&iITunes)))
		{
			long length(0), elapsed;
			
			//pTrack is a pointer to the track.  This gets passed to other functions to get track data.  wasTrack lets you check if the track was grabbed.
			IITTrack *pTrack = 0;
			//Sanity check -- should never fail if CoCreateInstance succeeded.  You may want to use this for debug output if it does ever fail.
			if (SUCCEEDED(iITunes->get_CurrentTrack(&pTrack)) && pTrack != NULL)
			{
				//Get album, then call ::COLE2T() to convert the text to array
				BSTR album;
				pTrack->get_Album(&album);
				if (album != NULL)
				{
					::COLE2T Album(album);
					params["album"] = Text::fromT(Album.m_szBuffer);
				}
				
				//Same for artist
				BSTR artist;
				pTrack->get_Artist(&artist);
				if (artist != NULL)
				{
					::COLE2T Artist(artist);
					params["artist"] = Text::fromT(Artist.m_szBuffer);
				}
				
				//Track name (get_Name is inherited from IITObject, of which IITTrack is derived)
				BSTR name;
				pTrack->get_Name(&name);
				if (name != NULL)
				{
					::COLE2T Name(name);
					params["title"] = Text::fromT(Name.m_szBuffer);
				}
				
				// Genre
				BSTR genre;
				pTrack->get_Genre(&genre);
				if (genre != NULL)
				{
					::COLE2T Genre(name);
					params["genre"] = Text::fromT(Genre.m_szBuffer);
				}
				
				//Total song time
				pTrack->get_Duration(&length);
				if (length > 0)
				{
					params["length"] = Util::formatSeconds(length);    // <--- once more with feeling
				}
				
				//Bitrate
				long bitrate;
				pTrack->get_BitRate(&bitrate);
				if (bitrate > 0)
				{
					params["bitrate"] = Util::toString(bitrate) + "kbps";    //<--- I'm not gonna play those games.  Mind games.  Board games.  I'm like, come on fhqugads...
				}
				
				//Frequency
				long frequency;
				pTrack->get_SampleRate(&frequency);
				if (frequency > 0)
				{
					params["frequency"] = Util::toString(frequency / 1000) + "kHz";
				}
				
				//Year
				long year;
				pTrack->get_Year(&year);
				if (year > 0)
				{
					params["year"] = Util::toString(year);
				}
				
				//Size
				long size;
				pTrack->get_Size(&size);
				if (size > 0)
				{
					params["size"] = Util::formatBytes(int64_t(size));
				}
				
				//Release (decrement reference count to 0) track object so it can unload and free itself; otherwise, it's locked in memory.
				safe_release(pTrack);
			}
			
			//Player status (stopped, playing, FF, rewind)
			ITPlayerState pStatus;
			iITunes->get_PlayerState(&pStatus);
			if (pStatus == ITPlayerStateStopped)
			{
				params["state"] = "stopped";
			}
			else if (pStatus == ITPlayerStatePlaying)
			{
				params["state"] = "playing";
			}
			
			//Player position (in seconds, you'll want to convert for your output)
			iITunes->get_PlayerPosition(&elapsed);
			if (elapsed > 0)
			{
				params["elapsed"] = Util::formatSeconds(elapsed);
				int intPercent;
				if (length > 0)
				{
					intPercent = elapsed * 100 / length;
				}
				else
				{
					length = 0;
					intPercent = 0;
				}
				params["percent"] = Util::toStringPercent(intPercent);
				int numFront = min(max(intPercent / 10, 0), 10),
				    numBack = min(max(10 - 1 - numFront, 0), 10);
				string inFront = string(numFront, '-'),
				       inBack = string(numBack, '-');
				params["bar"] = '[' + inFront + (elapsed > 0 ? '|' : '-') + inBack + ']';
			}
			
			//iTunes version
			BSTR version;
			iITunes->get_Version(&version);
			if (version != NULL)
			{
				::COLE2T iVersion(version);
				params["version"] = Text::fromT(iVersion.m_szBuffer);
			}
			
			//Release (decrement reference counter to 0) IiTunes object so it can unload and free itself; otherwise, it's locked in memory
			safe_release(iITunes);
		}
		
		//unload COM library -- this is also essential to prevent leaks and to keep it working the next time.
		CoUninitialize();
		
		// If there is something in title, we have at least partly succeeded
		if (!params["title"].empty())
		{
			return Util::formatParams(SETTING(ITUNES_FORMAT), params, false);
		}
		else
		{
			return "no_media";
		}
	}
	else
	{
		return "";
	}
}

// mpc = mplayerc = MediaPlayer Classic
// liberally inspired (copied with changes) by the GPLed application mpcinfo by Gabest
string Players::getMPCSpam()
{
	StringMap params;
	bool success = false;
	CComPtr<IRunningObjectTable> pROT;
	CComPtr<IEnumMoniker> pEM;
	CComQIPtr<IFilterGraph> pFG;
	if (SUCCEEDED(GetRunningObjectTable(0, &pROT)) && SUCCEEDED(pROT->EnumRunning(&pEM)))
	{
		CComPtr<IBindCtx> pBindCtx;
		if (SUCCEEDED(CreateBindCtx(0, &pBindCtx)))
			for (CComPtr<IMoniker> pMoniker; SUCCEEDED(pEM->Next(1, &pMoniker, NULL)); pMoniker = NULL)
			{
				if (!pMoniker)
					break;
				LPOLESTR pDispName = NULL;
				if (!SUCCEEDED(pMoniker->GetDisplayName(pBindCtx, NULL, &pDispName)))
					continue;
				wstring strw(pDispName);
				CComPtr<IMalloc> pMalloc;
				if (!SUCCEEDED(CoGetMalloc(1, &pMalloc)))
					continue;
				pMalloc->Free(pDispName);
				// Prefix string literals with the L character to indicate a UNCODE string.
				if (strw.find(L"(MPC)") == wstring::npos)
					continue;
				CComPtr<IUnknown> pUnk;
				if (!SUCCEEDED(pROT->GetObject(pMoniker, &pUnk)))
					continue;
				pFG = pUnk;
				if (!pFG)
					continue;
				success = true;
				break;
			}
			
		if (success)
		{
			// file routine (contains size routine)
			CComPtr<IEnumFilters> pEF;
			if (SUCCEEDED(pFG->EnumFilters(&pEF)))
			{
				// from the file routine
				ULONG cFetched = 0;
				for (CComPtr<IBaseFilter> pBF; SUCCEEDED(pEF->Next(1, &pBF, &cFetched)); pBF = NULL)
				{
					if (CComQIPtr<IFileSourceFilter> pFSF = pBF)
					{
						LPOLESTR pFileName = NULL;
						AM_MEDIA_TYPE mt;
						if (SUCCEEDED(pFSF->GetCurFile(&pFileName, &mt)))
						{
							// second parameter is a AM_MEDIA_TYPE structure, which contains codec info
							//                                      LPSTR thisIsBad = (LPSTR) pFileName;
							AutoArray<char> test(1000);
							WideCharToMultiByte(CP_ACP, 0, pFileName, -1, test, 1000, NULL, NULL);
							string filename(test);
							params["filepath"] = filename;
							params["filename"] = Util::getFileName(filename); //otherwise fully qualified
							params["title"] = params["filename"].substr(0, params["filename"].size() - 4);
							params["size"] = Util::formatBytes(File::getSize(filename));
							
							if (BOOLSETTING(USE_MAGNETS_IN_PLAYERS_SPAM))
							{
								string magnet = Util::getMagnetByPath(params["filepath"]);
								if (magnet.length() > 0)
									params["magnet"] = magnet;
								else
#if defined( SSA_DONT_SHOW_MAGNET_ON_NO_FILE_IN_SHARE )
									params["magnet"] = "";
#else
									params["magnet"] = params["filename"];
#endif
							}
							
							CoTaskMemFree(pFileName);
							// alternative to FreeMediaType(mt)
							// provided by MSDN DirectX 9 help page for FreeMediaType
							if (mt.cbFormat != 0)
							{
								CoTaskMemFree((PVOID)mt.pbFormat);
								mt.cbFormat = 0;
								mt.pbFormat = NULL;
							}
							if (mt.pUnk != NULL)
							{
								// Unecessary because pUnk should not be used, but safest.
								safe_release(mt.pUnk);
							}
							// end provided by MSDN
							break;
						}
					}
				}
			}
			
			// paused / stopped / running?
			CComQIPtr<IMediaControl> pMC;
			OAFilterState fs = 0;
			int state = 0;
			if ((pMC = pFG) && SUCCEEDED(pMC->GetState(0, &fs)))
			{
				switch (fs)
				{
					case State_Running:
						params["state"] = "playing";
						state = 1;
						break;
					case State_Paused:
						params["state"] = "paused";
						state = 3;
						break;
					case State_Stopped:
						params["state"] = "stopped";
						state = 0;
				};
			}
			
			// position routine
			CComQIPtr<IMediaSeeking> pMS = pFG;
			REFERENCE_TIME pos = 0, dur = 0;
			if (SUCCEEDED(pMS->GetCurrentPosition(&pos)) && SUCCEEDED(pMS->GetDuration(&dur)))
			{
				params["elapsed"] = Util::formatSeconds(pos / 10000000);
				params["length"] = Util::formatSeconds(dur / 10000000);
				int intPercent = 0;
				if (dur != 0)
					intPercent = (int)(pos * 100 / dur);
				params["percent"] = Util::toStringPercent(intPercent);
				int numFront = min(max(intPercent / 10, 0), 10),
				    numBack = min(max(10 - 1 - numFront, 0), 10);
				string inFront = string(numFront, '-'),
				       inBack = string(numBack, '-');
				params["bar"] = '[' + inFront + (state ? '|' : '-') + inBack + ']';
			}
			/*
			*   "+me watches: %[filename] (%[elapsed]/%[length]) Size: %[size]"
			*   from mpcinfo.txt
			*   "+me is playing %[filename] %[size] (%[elapsed]/%[length]) Media Player Classic %[version]"
			*   from http://www.faireal.net/articles/6/25/
			*/
		}
	}
	
	if (success)
	{
		return Util::formatParams(SETTING(MPLAYERC_FORMAT), params, false);
	}
	else
	{
		return Util::emptyString;
	}
}

/*
string Players::getSpotifySpam(HWND playerWnd) {
    if(playerWnd != NULL) {
        ParamMap params;
        TCHAR titleBuffer[2048];
        GetWindowText(playerWnd, titleBuffer, sizeof(titleBuffer));
        tstring title = titleBuffer;
        if (strcmpi(Text::fromT(title).c_str(), "Spotify") == 0)
            return "no_media";
        boost::algorithm::replace_first(title, "Spotify - ", "");
        params["title"] = Text::fromT(title);

        TCHAR appDataPath[MAX_PATH];
        ::SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appDataPath);
        string line;
        boost::regex ritem;
        boost::regex rname;
        boost::regex rhash;

        string path = Text::fromT(appDataPath) + "\\Spotify\\Users\\";
        ifstream guistate;

        ritem.assign("\"name\":\"([^\"]+)\",\"uri\":\"([^\"]+)");
        rname.assign("(?<=\"name\":\")([^\"]+)");
        rhash.assign("(?<=spotify:track:)([^\"]+)");
        params["link"] = Util::emptyString;

        StringList usersList = File::findFiles(path, "*");

        //find the file containing the latest songs played
        for(auto& u: usersList) {
            auto userFiles = File::findFiles(u, "guistate");
            for(auto& file: userFiles) {
                guistate.open(Text::utf8ToAcp(file));
                while (getline(guistate, line)) {
                    boost::smatch result;
                    string::const_iterator begin=line.begin(), end=line.end();
                    //find each song
                    while (regex_search(begin, end, result, ritem)) {
                        std::string item( result[0].first, result[0].second );
                        boost::smatch nresult;
                        //compare the name with the song being played now
                        if (regex_search(item, nresult, rname)) {
                            std::string song( nresult[0].first, nresult[0].second );
                            if (Text::fromT(title).find(song) != tstring::npos) {
                                //current song found, get the hash
                                boost::smatch hresult;
                                if (regex_search(item, hresult, rhash)) {
                                    std::string hash( hresult[0].first, hresult[0].second );
                                    params["link"] = "spotify:track:" + hash;
                                    break;
                                }
                            }
                        }
                        begin = result[0].second;
                    }
                }
            }
        }

        return Util::formatParams(SETTING(SPOTIFY_FORMAT), params);
    }

    return Util::emptyString;
}
*/

// This works for WMP 9+, but it's little slow, but hey we're talking about an MS app here, so what can you expect from the remote support for it...
string Players::getWMPSpam(HWND playerWnd /*= NULL*/, HWND g_mainWnd /*= NULL*/)
{
	// If it's not running don't even bother...
	if (playerWnd != NULL)
	{
		// Load COM
		CoInitialize(NULL);
		
		// Pointers
		CComPtr<IWMPPlayer>                 Player;
		CComPtr<IAxWinHostWindow>           Host;
		CComPtr<IObjectWithSite>            HostObj;
		CComObject<WMPlayerRemoteApi>       *WMPlayerRemoteApiCtrl = nullptr;
		
		// Other
		CAxWindow *DummyWnd = 0;
		StringMap params;
		
		// Create hidden window to host the control (if there just was other way to do this... as CoCreateInstance has no access to the current running instance)
		AtlAxWinInit();
		DummyWnd = new CAxWindow();
		DummyWnd->Create(g_mainWnd, NULL, NULL, WS_CHILD | WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SIZEBOX | WS_SYSMENU);
		HRESULT hresult = ::IsWindow(DummyWnd->m_hWnd) ? S_OK : E_FAIL;
		
		// Set WMPlayerRemoteApi
		if (SUCCEEDED(hresult))
		{
			hresult = DummyWnd->QueryHost(IID_IObjectWithSite, (void **) & HostObj);
			hresult = HostObj.p ? hresult : E_FAIL;
			
			if (SUCCEEDED(hresult))
			{
				hresult = CComObject<WMPlayerRemoteApi>::CreateInstance(&WMPlayerRemoteApiCtrl);
				if (WMPlayerRemoteApiCtrl)
				{
					WMPlayerRemoteApiCtrl->AddRef();
				}
				else
				{
					hresult = E_POINTER;
				}
			}
			
			if (SUCCEEDED(hresult))
			{
				hresult = HostObj->SetSite((IWMPRemoteMediaServices *)WMPlayerRemoteApiCtrl);
			}
		}
		
		if (SUCCEEDED(hresult))
		{
			hresult = DummyWnd->QueryHost(&Host);
			hresult = Host.p ? hresult : E_FAIL;
		}
		
		// Create WMP Control
		if (SUCCEEDED(hresult))
		{
			hresult = Host->CreateControl(CComBSTR(L"{6BF52A52-394A-11d3-B153-00C04F79FAA6}"), DummyWnd->m_hWnd, 0);
		}
		
		// Now we can finally start to interact with WMP, after we successfully get the "Player"
		if (SUCCEEDED(hresult))
		{
			hresult = DummyWnd->QueryControl(&Player);
			hresult = Player.p ? hresult : E_FAIL;
		}
		
		// We've got this far now the grande finale, get the metadata :)
		if (SUCCEEDED(hresult))
		{
			CComPtr<IWMPMedia>      Media;
			CComPtr<IWMPControls>   Controls;
			
			if (SUCCEEDED(Player->get_currentMedia(&Media)) && Media.p != NULL)
			{
				Player->get_controls(&Controls);
				
				// Windows Media Player version
				CComBSTR bstrWMPVer;
				Player->get_versionInfo(&bstrWMPVer);
				if (bstrWMPVer.Length() != 0)
				{
					::COLE2T WMPVer(bstrWMPVer);
					params["fullversion"] = Text::fromT(WMPVer.m_szBuffer);
					params["version"] = params["fullversion"].substr(0, params["fullversion"].find("."));
				}
				
				// Pre-formatted status message from Windows Media Player
				CComBSTR bstrWMPStatus;
				Player->get_status(&bstrWMPStatus);
				if (bstrWMPStatus.Length() != 0)
				{
					::COLE2T WMPStatus(bstrWMPStatus);
					params["status"] = Text::fromT(WMPStatus.m_szBuffer);
				}
				
				// Name of the currently playing media
				CComBSTR bstrMediaName;
				Media->get_name(&bstrMediaName);
				if (bstrMediaName.Length() != 0)
				{
					::COLE2T MediaName(bstrMediaName);
					params["title"] = Text::fromT(MediaName.m_szBuffer);
				}
				
				CComBSTR bstrMediaURL;
				Media->get_sourceURL(&bstrMediaURL);
				if (bstrMediaURL.Length() != 0)
				{
					::COLE2T MediaURL(bstrMediaURL);
					params["filepath"] = Text::fromT(MediaURL.m_szBuffer);
				}
				
				if (BOOLSETTING(USE_MAGNETS_IN_PLAYERS_SPAM))
				{
					string magnet = Util::getMagnetByPath(params["filepath"]);
					if (magnet.length() > 0)
						params["magnet"] = magnet;
					else
#if defined( SSA_DONT_SHOW_MAGNET_ON_NO_FILE_IN_SHARE )
						params["magnet"] = "";
#else
						params["magnet"] = Util::getFileName(params["filepath"]);
#endif
						
				}
				
				
				// How much the user has already played
				// I know this is later duplicated with get_currentPosition() for percent and bar, but for some reason it's overall faster this way
				CComBSTR bstrMediaPosition;
				Controls->get_currentPositionString(&bstrMediaPosition);
				if (bstrMediaPosition.Length() != 0)
				{
					::COLE2T MediaPosition(bstrMediaPosition);
					params["elapsed"] = Text::fromT(MediaPosition.m_szBuffer);
				}
				
				// Full duratiuon of the media
				// I know this is later duplicated with get_duration() for percent and bar, but for some reason it's overall faster this way
				CComBSTR bstrMediaDuration;
				Media->get_durationString(&bstrMediaDuration);
				if (bstrMediaDuration.Length() != 0)
				{
					::COLE2T MediaDuration(bstrMediaDuration);
					params["length"] = Text::fromT(MediaDuration.m_szBuffer);
				}
				
				// Name of the artist (use Author as secondary choice)
				CComBSTR bstrArtistName;
				Media->getItemInfo(CComBSTR(_T("WM/AlbumArtist")), &bstrArtistName);
				if (bstrArtistName.Length() != 0)
				{
					::COLE2T ArtistName(bstrArtistName);
					params["artist"] = Text::fromT(ArtistName.m_szBuffer);
				}
				else
				{
					Media->getItemInfo(CComBSTR(_T("Author")), &bstrArtistName);
					if (bstrArtistName.Length() != 0)
					{
						::COLE2T ArtistName(bstrArtistName);
						params["artist"] = Text::fromT(ArtistName.m_szBuffer);
					}
				}
				
				// Name of the album
				CComBSTR bstrAlbumTitle;
				Media->getItemInfo(CComBSTR(_T("WM/AlbumTitle")), &bstrAlbumTitle);
				if (bstrAlbumTitle.Length() != 0)
				{
					::COLE2T AlbumName(bstrAlbumTitle);
					params["album"] = Text::fromT(AlbumName.m_szBuffer);
				}
				
				// Genre of the media
				CComBSTR bstrMediaGen;
				Media->getItemInfo(CComBSTR(_T("WM/Genre")), &bstrMediaGen);
				if (bstrMediaGen.Length() != 0)
				{
					::COLE2T MediaGen(bstrMediaGen);
					params["genre"] = Text::fromT(MediaGen.m_szBuffer);
				}
				
				// Year of publiciation
				CComBSTR bstrMediaYear;
				Media->getItemInfo(CComBSTR(_T("WM/Year")), &bstrMediaYear);
				if (bstrMediaYear.Length() != 0)
				{
					::COLE2T MediaYear(bstrMediaYear);
					params["year"] = Text::fromT(MediaYear.m_szBuffer);
				}
				else
				{
					Media->getItemInfo(CComBSTR(_T("ReleaseDateYear")), &bstrMediaYear);
					if (bstrMediaYear.Length() != 0)
					{
						::COLE2T MediaYear(bstrMediaYear);
						params["year"] = Text::fromT(MediaYear.m_szBuffer);
					}
				}
				
				// Bitrate, displayed as Windows Media Player displays it
				CComBSTR bstrMediaBitrate;
				Media->getItemInfo(CComBSTR(_T("Bitrate")), &bstrMediaBitrate);
				if (bstrMediaBitrate.Length() != 0)
				{
					::COLE2T MediaBitrate(bstrMediaBitrate);
					double BitrateAsKbps = (Util::toDouble(Text::fromT(MediaBitrate.m_szBuffer)) / 1000);
					params["bitrate"] = Util::toString(int(BitrateAsKbps)) + "kbps";
				}
				
				// Size of the file
				CComBSTR bstrMediaSize;
				Media->getItemInfo(CComBSTR(_T("Size")), &bstrMediaSize);
				if (bstrMediaSize.Length() != 0)
				{
					::COLE2T MediaSize(bstrMediaSize);
					params["size"] = Util::formatBytes(Text::fromT(MediaSize.m_szBuffer));
				}
				
				// Users rating for this media
				CComBSTR bstrUserRating;
				Media->getItemInfo(CComBSTR(_T("UserRating")), &bstrUserRating);
				if (bstrUserRating.Length() != 0)
				{
					if (bstrUserRating == "0")
					{
						params["rating"] = "unrated";
					}
					else if (bstrUserRating == "1")
					{
						params["rating"] = "*";
					}
					else if (bstrUserRating == "25")
					{
						params["rating"] = "* *";
					}
					else if (bstrUserRating == "50")
					{
						params["rating"] = "* * *";
					}
					else if (bstrUserRating == "75")
					{
						params["rating"] = "* * * *";
					}
					else if (bstrUserRating == "99")
					{
						params["rating"] = "* * * * *";
					}
					else
					{
						params["rating"] = Util::emptyString;
					}
				}
				
				// Bar & percent
				double elapsed = 0;
				double length = 0;
				Controls->get_currentPosition(&elapsed);
				Media->get_duration(&length);//TODO: on WMP 12 may be 0.01, need fix
				if (elapsed > 0 && length >= elapsed)
				{
					int intPercent;
					if (int(length) > 0)
					{
						intPercent = int(elapsed * 100.0 / length);
					}
					else
					{
						length = 0;
						intPercent = 0;
					}
					params["percent"] = Util::toStringPercent(intPercent);
					int numFront = min(max(intPercent / 10, 0), 10),
					    numBack = min(max(10 - 1 - numFront, 0), 10);
					string inFront = string(numFront, '-'),
					       inBack = string(numBack, '-');
					params["bar"] = '[' + inFront + (elapsed > 0 ? '|' : '-') + inBack + ']';
				}
				else
				{
					params["percent"] = "0%";
					params["bar"] = "[|---------]";
				}
			}
		}
		
		// Release WMPlayerRemoteApi, if it's there
		safe_release(WMPlayerRemoteApiCtrl);
		// Destroy the hoster window, and unload COM
		DummyWnd->DestroyWindow();
		delete DummyWnd;
		CoUninitialize();
		
		// If there is something in title, we have at least partly succeeded
		if (!params["title"].empty())
		{
			return Util::formatParams(SETTING(WMP_FORMAT), params, false);
		}
		else
		{
			return "no_media";
		}
	}
	else
	{
		return Util::emptyString;
	}
}

string Players::getWinampSpam(HWND playerWnd, int playerType)
{
	// playerType : 0 - WinAmp, 1 - QCD/QMP
	if (playerWnd)
	{
		StringMap params;
		int majorVersion, minorVersion;
		string version;
		if (playerType == 1)
		{
			string buildVersion = Util::toString(SendMessage(playerWnd, WM_QCD_GETVERSION, 0, 1));
			int waVersion = SendMessage(playerWnd, WM_QCD_GETVERSION, 0, 0);
			majorVersion = waVersion >> 16;
			//  majorVersion = (DWORD)(HIBYTE(LOWORD(waVersion)));
			minorVersion = (DWORD)(LOBYTE(LOWORD(waVersion)));
			
			//wchar_buf_length = SendMessage(playerWnd, WM_QCD_GETGENRE, wchar_buf, MAX_PATH);
			//params["genre"] =
			
			version = Util::toString(majorVersion) + "." + Util::toString(minorVersion);
			if (majorVersion > 4)
				version += "." + buildVersion;
			if (FindWindow(_T("Winamp v1.x"), NULL))
				playerWnd = FindWindow(_T("Winamp v1.x"), NULL);
		}
		else
		{
			int waVersion = SendMessage(playerWnd, WM_USER, 0, IPC_GETVERSION);
			majorVersion = waVersion >> 12;
			if (((waVersion & 0x00F0) >> 4) == 0)
			{
				minorVersion = ((waVersion & 0x0f00) >> 8) * 10 + (waVersion & 0x000f);
			}
			else
			{
				minorVersion = ((waVersion & 0x00f0) >> 4) * 10 + (waVersion & 0x000f);
			}
			version = Util::toString(majorVersion) + "." + Util::toString(minorVersion);
		}
		params["version"] = version;
		int state = SendMessage(playerWnd, WM_USER, 0, IPC_ISPLAYING);
		switch (state)
		{
			case 0:
				params["state"] = "stopped";
				break;
			case 1:
				params["state"] = "playing";
				break;
			case 3:
				params["state"] = "paused";
		};
		const int buffLength = 1024;
		AutoArray<TCHAR> titleBuffer(buffLength);
		::GetWindowText(playerWnd, titleBuffer.data(), buffLength);
		tstring title = titleBuffer.data();
		params["rawtitle"] = Text::fromT(title);
#if defined (SSA_NEW_WINAMP_PROC_FOR_TITLE_AND_FILENAME)
		// [+] SSA
		const int idx = SendMessage(playerWnd, WM_WA_IPC, 0, IPC_GETLISTPOS);
		if (idx >= 0)
		{
			DWORD winampHandle = 0;
			GetWindowThreadProcessId(playerWnd, &winampHandle);
			HANDLE hWinamp = OpenProcess(PROCESS_ALL_ACCESS, false, winampHandle);
			AutoArray<char> name(MAX_PATH);
			name[0] = 0;
			SIZE_T name_len = 0;
			LPCVOID lpFileName = (LPCVOID)SendMessage(playerWnd, WM_USER, idx, IPC_GETPLAYLISTFILE);
			if (lpFileName)
				ReadProcessMemory(hWinamp, lpFileName, name.data(), MAX_PATH, &name_len);
			AutoArray<char> realtitle(MAX_PATH);
			realtitle[0] = 0;
			SIZE_T title_len = 0;
			LPCVOID lpFileTitle = (LPCVOID)SendMessage(playerWnd, WM_USER, idx, IPC_GETPLAYLISTTITLE);
			if (lpFileTitle)
				ReadProcessMemory(hWinamp, lpFileTitle, realtitle.data(), MAX_PATH, &title_len);
				
			CloseHandle(hWinamp);
			
			params["title"]  = Text::acpToUtf8(realtitle.data());
			
			if (BOOLSETTING(USE_MAGNETS_IN_PLAYERS_SPAM))
			{
				params["filepath"]  = Text::acpToUtf8(name.data());
				const string magnet = Util::getMagnetByPath(params["filepath"]);
				if (magnet.length() > 0)
					params["magnet"] = magnet;
				else
#if defined( SSA_DONT_SHOW_MAGNET_ON_NO_FILE_IN_SHARE )
					params["magnet"] = "";
#else
					params["magnet"] = Util::getFileName(params["filepath"]);
#endif
			}
		}
#else
		// there's some winamp bug with scrolling. wa5.09x and 5.1 or something.. see:
		// http://forums.winamp.com/showthread.php?s=&postid=1768775#post1768775
		tstring::size_type starpos = title.find(_T("***"));
		if (starpos != tstring::npos && starpos >= 1)
		{
			tstring firstpart = title.substr(0, starpos - 1);
			if (firstpart == title.substr(title.size() - firstpart.size(), title.size()))
			{
				// fix title
				title = title.substr(starpos);
			}
		}
		// fix the title if scrolling is on, so need to put the stairs to the end of it
		tstring titletmp;
		if (title.find(_T("***")) != tstring::npos)
			titletmp = title.substr(title.find(_T("***")) + 2, title.size());
		title = titletmp + title.substr(0, title.size() - titletmp.size());
		if (title.find(_T('.'))  != tstring::npos)
			title = title.substr(title.find(_T('.')) + 2, title.size());
		if (title.rfind(_T('-')) != tstring::npos)
		{
			params["title"] = Text::fromT(title.substr(0, title.rfind(_T('-')) - 1));
		}
#endif
		int curPos = SendMessage(playerWnd, WM_USER, 0, IPC_GETOUTPUTTIME);
		int length = SendMessage(playerWnd, WM_USER, 1, IPC_GETOUTPUTTIME);
		if (curPos == -1)
		{
			curPos = 0;
		}
		else
		{
			curPos /= 1000;
		}
		int intPercent;
		if (length > 0)
		{
			intPercent = curPos * 100 / length;
		}
		else
		{
			length = 0;
			intPercent = 0;
		}
		params["percent"] = Util::toStringPercent(intPercent);
		params["elapsed"] = Util::formatSeconds(curPos, true);
		params["length"] = Util::formatSeconds(length, true);
		const int numFront = min(max(intPercent / 10, 0), 10),
		          numBack = min(max(10 - 1 - numFront, 0), 10);
		string inFront = string(numFront, '-'),
		       inBack = string(numBack, '-');
		params["bar"] = '[' + inFront + (state ? '|' : '-') + inBack + ']';
		int waSampleRate = SendMessage(playerWnd, WM_USER, 0, IPC_GETINFO),
		    waBitRate = SendMessage(playerWnd, WM_USER, 1, IPC_GETINFO),
		    waChannels = SendMessage(playerWnd, WM_USER, 2, IPC_GETINFO);
		//[!] SSA fix for QCD
		if (playerType == 1 || BOOLSETTING(USE_BITRATE_FIX_FOR_SPAM))
		{
			if (waSampleRate > 100) // if in Herz
			{
				waSampleRate = waSampleRate / 1000.0;
			}
			if (waBitRate > 10000)  // if in Bits
			{
				waBitRate = waBitRate / 1000.0;
			}
		}
		//[!] SSA fix for AIMP
		params["bitrate"] = Util::toString(waBitRate) + "kbps";
		params["sample"] = Util::toString(waSampleRate) + "kHz";
		// later it should get some improvement:
		string waChannelName;
		switch (waChannels)
		{
			case 2:
				waChannelName = "stereo";
				break;
			case 6:
				waChannelName = "5.1 surround";
				break;
			default:
				waChannelName = "mono";
		}
		params["channels"] = waChannelName;
		//params["channels"] = (waChannels==2?"stereo":"mono"); // 3+ channels? 0 channels?
		if (playerType != 1)    //QCD
		{
			return Util::formatParams(SETTING(WINAMP_FORMAT), params, false);
		}
		else
		{
			return Util::formatParams(SETTING(QCDQMP_FORMAT), params, false);
		}
	}
	else
	{
		return Util::emptyString;
	}
}


string Players::getJASpam()
{
	MainFrame* mFrame = MainFrame::getMainFrame();
	if (!mFrame)
		return Util::emptyString;
	JAControl* jaControl = mFrame->getJAControl();
	if (!jaControl || !jaControl->isJARunning())
		return Util::emptyString;
	StringMap params;
	jaControl->JAUpdateAllInfo();
	params["filepath"] = Text::acpToUtf8(jaControl->getJATrackFileName());
	params["title"] = Text::acpToUtf8(jaControl->getJATrackTitle());
	params["artist"] = Text::acpToUtf8(jaControl->getJATrackArtist());
	if (jaControl->isJAPaused())
		params["state"] = "paused";
	else if (jaControl->isJAPlaying())
		params["state"] = "playing";
	else if (jaControl->isJAStopped())
		params["state"] = "stopped";
		
	params["elapsed"] = Text::acpToUtf8(jaControl->getJACurrTimeString());
	int intPercent  = jaControl->getJATrackProgress() * 100;
	params["percent"] = Util::toStringPercent(intPercent);
	
	int numFront = min(max(intPercent / 10, 0), 10),
	    numBack = min(max(10 - 1 - numFront, 0), 10);
	string inFront = string(numFront, '-'),
	       inBack = string(numBack, '-');
	params["bar"] = Text::acpToUtf8('[' + inFront + (jaControl->isJAPlaying() ? '|' : '-') + inBack + ']');
	
	params["length"] = Text::acpToUtf8(jaControl->getJACurrTimeString(false));
	params["version"] = Text::acpToUtf8(jaControl->getJAVersion());
	if (BOOLSETTING(USE_MAGNETS_IN_PLAYERS_SPAM))
	{
		string magnet = Util::getMagnetByPath(params["filepath"]);
		if (magnet.length() > 0)
			params["magnet"] = magnet;
		else
#if defined( SSA_DONT_SHOW_MAGNET_ON_NO_FILE_IN_SHARE )
			params["magnet"] = "";
#else
			params["magnet"] = Util::getFileName(params["filepath"]);
#endif
	}
	return Util::formatParams(SETTING(JETAUDIO_FORMAT), params, false);
}
