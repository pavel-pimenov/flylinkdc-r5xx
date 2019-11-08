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

#ifndef _JA_CONTROL_H_
#define _JA_CONTROL_H_


#pragma once

#include <string>
#include <sstream>
#include "JetAudio6_API.h"


class JAControl
{
	public:
		explicit JAControl(HWND window);
		~JAControl() { }
		
	public:
		// JAUpdateAllInfo
		void JAUpdateAllInfo();
		bool ProcessCopyData(COPYDATASTRUCT* pCopyDataStruct);
		
	public:
		// Get Info
		std::string getJATrackTitle()
		{
			return m_trackTitle;
		}
		std::string getJATrackFileName()
		{
			return m_trackFilename;
		}
		std::string getJATrackArtist()
		{
			return m_trackArtist;
		}
		std::string getJAVersion()
		{
			return m_version;
		}
		
		double getJATrackCurrTimeSec()
		{
			return (double)m_currTimeMS / 1000;
		}
		double getJATrackLengthTimeSec()
		{
			return (double)m_maxTimeMS / 1000;
		}
		std::string getJACurrTimeString(bool isCurrentTime = true);
		float getJATrackProgress();
		int getJAVolume()
		{
			return m_currVolume;
		}
		int getJATrackNumber()
		{
			return m_curTrack;
		}
		int getJATracksCount()
		{
			return m_totalTracks;
		}
		
		bool isJARunning();
		bool isJAPlaying();
		bool isJAPaused();
		bool isJAStopped();
		
		// JA controls with Tracks
		void JANextTrack();
		void JAPrevTrack();
		
		void JASetStop();
		void JASetPlay(int trackID);
		void JASetPause();
		
		// Volume process
		void JASetVolume(int vol); // Sets the volume, clipping it between 0-100.
		void JAVolumeUp()
		{
			JASetVolume(m_currVolume + 10);
		}
		void JAVolumeDown()
		{
			JASetVolume(m_currVolume - 10);
		}
		void JAMute();
		
		// Exit
		void JAExit();
		void JAStart();
		
	private:
		HWND m_hJAudio;
		HWND m_hParent;
		
		int m_curTrack;
		int m_totalTracks;
		int m_currVolume;
		int m_currStatus;
		int m_maxTimeMS;
		int m_currTimeMS;
		
		std::string m_trackFilename;
		std::string m_trackTitle;
		std::string m_trackArtist;
		std::string m_version;
		
		bool m_isMuted;
		
	protected:
	
		void JAUpdateTrackInfo();
		void JAUpdateVolumeInfo();
		void JAUpdateTimeInfo();
		void JAUpdateStatusInfo();
		void JAUpdateVersionInfo();
};

#endif // _JA_CONTROL_H_