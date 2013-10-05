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

#ifdef IRAINMAN_INCLUDE_SMILE

#include "../client/Util.h"
#include "ImageDataObject.h"

class CGDIImage;
class CAGEmotion
{
	public:
		typedef CAGEmotion* Ptr;
		typedef std::vector<Ptr> Array;
		
		CAGEmotion();
		~CAGEmotion();
		
		void registerEmotion(const tstring& strEmotionText, const string& strEmotionBmpPath, const string& strEmotionGifPath);
		
		const tstring& getEmotionText() const
		{
			return m_EmotionText;
		}
		void initEmotionBmp();
		HBITMAP getEmotionBmp(const COLORREF &clrBkColor);
		bool getDimensions(int *pW, int *pH);
		const string& getEmotionBmpPath() const
		{
			return m_EmotionBmpPath;
		}
		const string& getEmotionGifPath() const
		{
			return m_EmotionGifPath;
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
		string      m_EmotionBmpPath;
		string      m_EmotionGifPath;
		HBITMAP     m_EmotionBmp;
		bool        m_EmotionBmpLoaded;
		bool        m_EmotionBmpLoadedError;
		long        m_ImagePos;
		static CImageList*  g_pImagesList;
		
		CImageDataObject m_BmpImageObject;
		CGDIImage *m_pGifImage;
		
		bool m_bMaySupportAnimation;
	private:
		// TODO не пашет в русских каталогах string  UnzipEmotions(const string& p_file_name);
		
};

// CAGEmotionSetup

class CAGEmotionSetup
#ifdef _DEBUG
	: virtual NonDerivable<CAGEmotionSetup>, boost::noncopyable // [+] IRainman fix.
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
		unordered_set<string> m_FilterEmotiion;
		void cleanup();
};

#endif // IRAINMAN_INCLUDE_SMILE

#endif // AGEMOTIONSETUP_H__
