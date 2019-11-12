/*
 *
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

#if !defined(AGEMOTIONSETUP_H__)
#define AGEMOTIONSETUP_H__

#pragma once


#ifdef IRAINMAN_INCLUDE_SMILE

#include "ImageDataObject.h"
#include <boost/unordered/unordered_set.hpp>

class CGDIImage;
class CAGEmotion
{
	public:
		typedef std::vector<CAGEmotion*> Array;
		
		CAGEmotion();
		~CAGEmotion();
		
		void registerEmotion(const tstring& strEmotionText, const string& strEmotionBmp, const string& strEmotionGif);
		
		const tstring& getEmotionText() const
		{
			return m_EmotionText;
		}
		void initEmotionBmp();
		HBITMAP getEmotionBmp(const COLORREF &clrBkColor);
		bool getDimensions(int *pW, int *pH);
		string getEmotionBmpPath() const
		{
			return Util::getEmoPacksPath() + m_EmotionBmp;
		}
		string getEmotionGifPath() const
		{
			return Util::getEmoPacksPath() + m_EmotionGif;
		}
		long  getImagePos() const
		{
			return m_ImagePos;
		}
		void setImagePos(long ImagePos)
		{
			m_ImagePos = ImagePos;
		}
		
		static void setImageList(CImageList* pImagesList)
		{
			g_pImagesList = pImagesList;
		}
		
		CGDIImage *getAnimatedImage(HWND hCallbackWnd, DWORD dwCallbackMsg);
		
		IOleObject *GetImageObject(bool bAnimated, IOleClientSite *pOleClientSite, IStorage *pStorage, HWND hCallbackWnd, DWORD dwCallbackMsg, COLORREF BkColor);
		
	protected:
		tstring     m_EmotionText;
		string      m_EmotionBmp;
		string      m_EmotionGif;
		HBITMAP     m_EmotionBmpHandle;
		bool        m_EmotionBmpLoaded;
		bool        m_EmotionBmpLoadedError;
		long        m_ImagePos;
		static CImageList*  g_pImagesList;
		
		CImageDataObject m_BmpImageObject;
		CGDIImage *m_pGifImage;
		
		bool m_bMaySupportAnimation;
	private:
		string UnzipEmotions(const string& p_file_name);
};

// CAGEmotionSetup

class CAGEmotionSetup
#ifdef _DEBUG
	:  boost::noncopyable // [+] IRainman fix.
#endif
{
	public:
		CAGEmotionSetup() : useEmoticons(false), m_CountSelEmotions(0) { }
		~CAGEmotionSetup();
		
		bool LoadEmotion(const string& p_file_name);
		bool InitImages();
		bool Create();
		//
		static bool reCreateEmotionSetup();
		static void destroyEmotionSetup();
		static CAGEmotionSetup* g_pEmotionsSetup;
		//
		// Variables
		GETSET(bool, useEmoticons, UseEmoticons);
		
		const CAGEmotion::Array& getEmoticonsArray() const
		{
			return m_EmotionsArray;
		}
		int m_CountSelEmotions; //[+]PPA
		
	protected:
		CImageList       m_EmotionImages;
		CAGEmotion::Array m_EmotionsArray;
	private:
		boost::unordered_set<string> m_FilterEmotion;
		void cleanup();
};

#endif // IRAINMAN_INCLUDE_SMILE

#endif // AGEMOTIONSETUP_H__
