/*
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
 *
 * Code updated by SSA
 */


#include "stdafx.h"
#include "JAControl.h"

#include "../client/Util.h"



JAControl::JAControl(HWND parent)
	: m_hParent(parent)
	, m_hJAudio(NULL)
	, m_curTrack(-1)
	, m_totalTracks(-1)
	, m_currVolume(-1)
	, m_currStatus(-1)
	, m_maxTimeMS(-1)
	, m_currTimeMS(-1), m_isMuted(false)
{
}

bool JAControl::ProcessCopyData(COPYDATASTRUCT* pCopyDataStruct)
{
	bool bProcessed = false;
	switch (pCopyDataStruct->dwData)
	{
		case JRC_COPYDATA_ID_TRACK_FILENAME:
		{
			m_trackFilename = (LPCSTR)pCopyDataStruct->lpData;
			bProcessed = true;
		}
		break;
		case JRC_COPYDATA_ID_TRACK_TITLE:
		{
			m_trackTitle = (LPCSTR)pCopyDataStruct->lpData;
			bProcessed = true;
		}
		break;
		case JRC_COPYDATA_ID_TRACK_ARTIST:
		{
			m_trackArtist = (LPCSTR)pCopyDataStruct->lpData;
			bProcessed = true;
		}
		break;
	}
	return bProcessed;
}

void JAControl::JAUpdateAllInfo()
{
	if (!isJARunning())
		return;
	JAUpdateTrackInfo();
	JAUpdateVolumeInfo();
	JAUpdateTimeInfo();
	JAUpdateStatusInfo();
	JAUpdateVersionInfo();
}

void JAControl::JAUpdateTrackInfo()
{

	if (!isJARunning())
		return;
		
	// string will be returned using WM_COPYDATA message
	::SendMessage(m_hJAudio, WM_REMOCON_GETSTATUS, (WPARAM) m_hParent, GET_STATUS_ALBUM_LIST);
	::SendMessage(m_hJAudio, WM_REMOCON_GETSTATUS, (WPARAM) m_hParent, GET_STATUS_ALBUM_INFO);
	::SendMessage(m_hJAudio, WM_REMOCON_GETSTATUS, (WPARAM) m_hParent, GET_STATUS_TRACK_FILENAME);
	::SendMessage(m_hJAudio, WM_REMOCON_GETSTATUS, (WPARAM) m_hParent, GET_STATUS_TRACK_TITLE);
	::SendMessage(m_hJAudio, WM_REMOCON_GETSTATUS, (WPARAM) m_hParent, GET_STATUS_TRACK_ARTIST);
	
	m_curTrack = ::SendMessage(m_hJAudio, WM_REMOCON_GETSTATUS, 0, GET_STATUS_CUR_TRACK); //-V103
	m_totalTracks = ::SendMessage(m_hJAudio, WM_REMOCON_GETSTATUS, 0, GET_STATUS_MAX_TRACK); //-V103
}


void JAControl::JAUpdateVersionInfo()
{
	int iVer1 = ::SendMessageA(m_hJAudio, WM_REMOCON_GETSTATUS, (WPARAM) m_hParent, GET_STATUS_JETAUDIO_VER1); //-V103
	int iVer2 = ::SendMessageA(m_hJAudio, WM_REMOCON_GETSTATUS, (WPARAM) m_hParent, GET_STATUS_JETAUDIO_VER2); //-V103
	int iVer3 = ::SendMessageA(m_hJAudio, WM_REMOCON_GETSTATUS, (WPARAM) m_hParent, GET_STATUS_JETAUDIO_VER3); //-V103
	
	m_version  = Util::toString(iVer1) + "." + Util::toString(iVer2) + "." + Util::toString(iVer3);
}

void JAControl::JAUpdateVolumeInfo()
{
	m_currVolume = ::SendMessage(m_hJAudio, WM_REMOCON_GETSTATUS, (WPARAM) m_hParent, GET_STATUS_VOLUME); //-V103
	m_isMuted = ::SendMessage(m_hJAudio, WM_REMOCON_GETSTATUS, (WPARAM) m_hParent, GET_STATUS_ATT) == 1;
}

void JAControl::JAUpdateTimeInfo()
{
	m_maxTimeMS = ::SendMessage(m_hJAudio, WM_REMOCON_GETSTATUS, (WPARAM) m_hParent, GET_STATUS_MAX_TIME); //-V103
	m_currTimeMS = ::SendMessage(m_hJAudio, WM_REMOCON_GETSTATUS, (WPARAM) m_hParent, GET_STATUS_CUR_TIME);  //-V103
}

void JAControl::JAUpdateStatusInfo()
{
	m_currStatus = ::SendMessage(m_hJAudio, WM_REMOCON_GETSTATUS, (WPARAM)m_hParent, GET_STATUS_STATUS); //-V103
}


float JAControl::getJATrackProgress()
{
	if (m_maxTimeMS == 0)
		return 0.0;
		
	return ((float)m_currTimeMS / m_maxTimeMS);
}

std::string JAControl::getJACurrTimeString(bool isCurrentTime)
{
	int currTimeSec =  isCurrentTime ? getJATrackCurrTimeSec() : getJATrackLengthTimeSec();
	std::string time;
	if ((float)currTimeSec / 3600 < 1)
		time = "0";
	else
		time = Util::toString(currTimeSec / 360);
	if (currTimeSec / 60 < 10)
		time += ":0" + Util::toString(currTimeSec / 60);
	else
		time += ":" + Util::toString(currTimeSec / 60);
	if (currTimeSec % 60 < 10)
		time += ":0" + Util::toString(currTimeSec % 60);
	else
		time += ":" + Util::toString(currTimeSec % 60);
	return time;
}



void JAControl::JASetVolume(int vol)
{
	if (vol > 100) vol = 100;
	if (vol < 0) vol = 0;
	SendMessage(m_hJAudio, WM_REMOCON_SENDCOMMAND, 0, MAKELPARAM(JRC_ID_SET_VOLUME, vol));
	JAUpdateVolumeInfo();
}

void JAControl::JASetStop()
{
	SendMessage(m_hJAudio, WM_REMOCON_SENDCOMMAND, 0, MAKELPARAM(JRC_ID_STOP, 0));
	JAUpdateStatusInfo();
}

void JAControl::JASetPlay(int trackNum)
{
	SendMessage(m_hJAudio, WM_REMOCON_SENDCOMMAND, 0, MAKELPARAM(JRC_ID_PLAY, trackNum));
	JAUpdateStatusInfo();
}

void JAControl::JASetPause()
{
	SendMessage(m_hJAudio, WM_REMOCON_SENDCOMMAND, 0, MAKELPARAM(JRC_ID_PLAY, 0));
	JAUpdateStatusInfo();
}


void JAControl::JANextTrack()
{
	::SendMessage(m_hJAudio, WM_REMOCON_SENDCOMMAND, 0, JRC_ID_NEXT_TRACK);
	JAUpdateTrackInfo();
}

void JAControl::JAPrevTrack()
{
	::SendMessage(m_hJAudio, WM_REMOCON_SENDCOMMAND, 0, JRC_ID_PREV_TRACK);
	JAUpdateTrackInfo();
}

void JAControl::JAMute()
{
	::SendMessage(m_hJAudio, WM_REMOCON_SENDCOMMAND, 0, JRC_ID_ATT);
	JAUpdateVolumeInfo();
}


void JAControl::JAExit()
{
	::SendMessage(m_hJAudio, WM_REMOCON_SENDCOMMAND, 0, JRC_ID_EXIT);
}
void JAControl::JAStart()
{
	::ShellExecute(m_hParent, L"open", L"jetaudio.exe", NULL, NULL, SW_SHOWDEFAULT);
}

bool JAControl::isJARunning()
{
	m_hJAudio = ::FindWindow(L"COWON Jet-Audio Remocon Class", L"Jet-Audio Remote Control");
	return ::IsWindow(m_hJAudio) != FALSE;
}

