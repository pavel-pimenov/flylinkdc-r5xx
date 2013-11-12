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

#include "stdafx.h"
#ifdef IRAINMAN_INCLUDE_SMILE
#include <math.h>
#include "../client/SimpleXML.h"
#include "../client/Pointer.h"
#include "../client/CompatibilityManager.h" // [+] IRainman
// #include "../client/ZipFile.h"
#include "../client/LogManager.h"
#include "../GdiOle/GDIImageOle.h"
#include "AGEmotionSetup.h"
//======================================================================================================================
CImageList* CAGEmotion::g_pImagesList = nullptr;
//======================================================================================================================
CAGEmotion::CAGEmotion() :
	m_EmotionBmp(nullptr),
	m_ImagePos(-1),
	m_bMaySupportAnimation(false),
	m_pGifImage(nullptr),
	m_EmotionBmpLoaded(false),
	m_EmotionBmpLoadedError(false)
{
}
//======================================================================================================================
CAGEmotion::~CAGEmotion()
{
	safe_release(m_pGifImage); // [10] https://www.box.net/shared/9uhvka1sqbu5344cbwm3
}
//======================================================================================================================
#ifdef _DEBUG
/*
string CAGEmotion::UnzipEmotions(const string& p_file_name)
{
//  CWaitCursor l_cursor_wait;
        // CFlyLog l_log("[Unzip EmoPacks.zip]" + p_file_name);
        try
        {
            dcpp::ZipFile zip;
            zip.Open("C:\\vc10\\r5xx\\compiled\\EmoPacks.zip");
            string srcPath = "C:\\vc10\\r5xx\\compiled\\EmoPacks.zip";
            string tempPath = Util::getTempPath();
            AppendPathSeparator(tempPath);
            tempPath += Util::getFileName(p_file_name);
            string dstPath = tempPath;
            string l_convert_name = p_file_name;
            for_each(begin(l_convert_name), end(l_convert_name), [](char& c)  {  c = c == '\\' ? '/' : c;});
            if (zip.GoToFirstFile())
            {
                do
                {
                    const string l_f_name = zip.GetCurrentFileName();
                    if (stricmp(l_f_name, l_convert_name) == 0)
                    {
                        zip.OpenCurrentFile();
                        zip.ReadCurrentFile(dstPath);
                        zip.CloseCurrentFile();
                        return dstPath;
                    }
                }
                while (zip.GoToNextFile());
                //dcassert(0);
                LogManager::getInstance()->message("UnzipEmotions, Error = "+ p_file_name);
            }
        }
        catch (dcpp::ZipFileException& e)
        {
            LogManager::getInstance()->message("UnzipEmotions, Error = " + e.getError());
        }
    return Util::emptyString;
}
*/
#endif // _DEBUG
//======================================================================================================================
void CAGEmotion::initEmotionBmp()
{
//  dcassert(m_EmotionBmpLoaded == false);
	if (m_EmotionBmpLoaded == false && m_EmotionBmpLoadedError == false) // Первый раз и нет ошибок загрузки
	{
		dcassert(m_EmotionBmpPath.size());
		if (m_EmotionBmpPath.size())
		{
			//UnzipEmotions(m_EmotionBmpPath);
			const std::string l_path = m_EmotionBmpPath;
			m_EmotionBmp = (HBITMAP)::LoadImage(0, Text::toT(l_path).c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
			//dcassert(m_EmotionBmp);
			if (m_EmotionBmp)
			{
				m_BmpImageObject.SetBitmap(m_EmotionBmp);
				CDC oTestDC;
				if (oTestDC.CreateCompatibleDC(NULL))
				{
					HBITMAP poPrevSourceBmp = (HBITMAP) SelectObject(oTestDC, m_EmotionBmp);
					COLORREF clrTransparent = GetPixel(oTestDC, 0, 0);
					SelectObject(oTestDC, poPrevSourceBmp);
					const int nImagePos = g_pImagesList->Add(m_EmotionBmp, clrTransparent); // [1] https://www.box.net/shared/dc4bb2f4d42173f47f62
					setImagePos(nImagePos);
					DeleteObject(m_EmotionBmp); // TODO не совсем понятно почему тут удаляется?
					m_EmotionBmp = nullptr;
					m_EmotionBmpLoaded = true;
					m_EmotionBmp = nullptr;
				}
				else
				{
					dcassert(0);
				}
			}
			else
			{
				m_EmotionBmpLoadedError = true;
				LogManager::getInstance()->message("CAGEmotion::initEmotionBmp, Error load = " + l_path);
			}
		}
	}
}
//======================================================================================================================
void CAGEmotion::registerEmotion(const tstring& strEmotionText, const string& strEmotionBmpPath, const string& strEmotionGifPath)
{
	m_EmotionText    = strEmotionText;
	m_EmotionBmpPath = strEmotionBmpPath;
	if (!CompatibilityManager::isWine())
		m_EmotionGifPath = strEmotionGifPath;
	dcassert(m_EmotionBmpPath.size());
//	if (m_EmotionBmpPath.size())
//	{
	//m_EmotionBmp = (HBITMAP)::LoadImage(0, Text::toT(m_EmotionBmpPath).c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	//if (m_EmotionBmp)
	//  m_BmpImageObject.SetBitmap(m_EmotionBmp);
//	}
	// m_bMaySupportAnimation = true;       //!!!
	if (!CompatibilityManager::isWine() && strEmotionGifPath.size())
	{
		m_bMaySupportAnimation = true;
		
		// [!] brain-ripper
		// Don't make any initialization here!
		// Any initialization will be done in
		// GetImageObject function, on demand,
		// to save resources and time for users, who don't
		// want use animated icons
	}
//	if (!m_EmotionBmp && !m_bMaySupportAnimation)
//	{
//		return false;
//	}
}
//======================================================================================================================
bool CAGEmotion::getDimensions(int *pW, int *pH)
{
	if (getImagePos() == -1)
	{
		initEmotionBmp();
	}
	bool bRet = false;
	if (g_pImagesList && getImagePos() >= 0)
	{
		IMAGEINFO ii;
		if (g_pImagesList->GetImageInfo(getImagePos(), &ii))
		{
			if (pW)
				*pW = ii.rcImage.right - ii.rcImage.left;
			if (pH)
				*pH = ii.rcImage.bottom - ii.rcImage.top;
				
			bRet = true;
		}
	}
	return bRet;
}
//======================================================================================================================
CGDIImage *CAGEmotion::getAnimatedImage(HWND hCallbackWnd, DWORD dwCallbackMsg)
{
	if (m_bMaySupportAnimation && !m_pGifImage)
	{
		// Create object and load icon only on demand,
		// to save resources and time for users, who don't
		// want use animated icons
		if (!m_EmotionGifPath.empty())
		{
			const std::string l_path = m_EmotionGifPath;
			// N.B. don't delete this object, use Release method!
			m_pGifImage = CGDIImage::CreateInstance(Text::toT(l_path).c_str(), hCallbackWnd, dwCallbackMsg);
			if (!m_pGifImage->IsInited())
			{
				LogManager::getInstance()->message("CAGEmotion::getAnimatedImage, Error init Gif = " + l_path);
				safe_release(m_pGifImage);
				m_bMaySupportAnimation = false;
			}
		}
	}
	return m_pGifImage;
}

IOleObject *CAGEmotion::GetImageObject(bool bAnimated, IOleClientSite *pOleClientSite, IStorage *pStorage, HWND hCallbackWnd, DWORD dwCallbackMsg, COLORREF BkColor)
{
	IOleObject *pObject = nullptr;
	
	if (bAnimated && (m_bMaySupportAnimation || m_pGifImage))
	{
		getAnimatedImage(hCallbackWnd, dwCallbackMsg);
		
		CComObject<CGDIImageOle> *pGifImageObject = nullptr;
		if (m_pGifImage)
		{
			// Create view instance of single graphic object (m_pGifImage).
			// Instance will be automatically removed after releasing last pointer;
			CComObject<CGDIImageOle>::CreateInstance(&pGifImageObject);
			
			if (pGifImageObject)
			{
				if (pGifImageObject->put_SetImage(m_pGifImage, BkColor, hCallbackWnd, dwCallbackMsg) != S_OK)
				{
					safe_delete(pGifImageObject);
					m_bMaySupportAnimation = false;
				}
			}
			else
				m_bMaySupportAnimation = false;
		}
		
		if (pGifImageObject)
		{
			pGifImageObject->QueryInterface(IID_IOleObject, (void**)&pObject);
			if (pObject) // PVS-Studio V595 The pointer was utilized before it was verified against nullptr.
				pObject->SetClientSite(pOleClientSite); //-V595
		}
		else
		{
			safe_release(m_pGifImage);
			m_bMaySupportAnimation = false;
		}
	}
	
	if (!pObject)
	{
		HBITMAP hBitmap = getEmotionBmp(BkColor); // leak
		
		if (hBitmap)
		{
			m_BmpImageObject.SetBitmap(hBitmap);
			pObject = m_BmpImageObject.GetOleObject(pOleClientSite, pStorage);
			
			DeleteObject(hBitmap);
		}
	}
	
	return pObject;
}

HBITMAP CAGEmotion::getEmotionBmp(const COLORREF &clrBkColor)
{
	if (g_pImagesList == nullptr)
		return NULL;
	if (getImagePos() == -1) // Индекса еще нет? - первый раз!
	{
		initEmotionBmp();
		dcassert(getImagePos() >= 0);
	}
	if (m_EmotionBmpLoadedError == false)
	{
		CBitmap dist;
		CClientDC dc(NULL);
		IMAGEINFO ii = {0};
		g_pImagesList->GetImageInfo(getImagePos(), &ii);
		const int nWidth = ii.rcImage.right - ii.rcImage.left;
		const int nHeight = ii.rcImage.bottom - ii.rcImage.top;
		dist.CreateCompatibleBitmap(dc, nWidth, nHeight); //
		/*
		flylinkdc_Debug.exe!WTL::CBitmapT<1>::CreateCompatibleBitmap Line 775 (c:\vc10\r5xx\wtl\atlgdi.h)
		flylinkdc_Debug.exe!CAGEmotion::getEmotionBmp Line 284 (c:\vc10\r5xx\windows\agemotionsetup.cpp)
		flylinkdc_Debug.exe!CAGEmotion::GetImageObject Line 252 (c:\vc10\r5xx\windows\agemotionsetup.cpp)
		flylinkdc_Debug.exe!ChatCtrl::AppendText Line 292 (c:\vc10\r5xx\windows\chatctrl.cpp)
		flylinkdc_Debug.exe!BaseChatFrame::addLine Line 625 (c:\vc10\r5xx\windows\basechatframe.cpp)
		flylinkdc_Debug.exe!HubFrame::addLine Line 2099 (c:\vc10\r5xx\windows\hubframe.cpp)
		flylinkdc_Debug.exe!HubFrame::onSpeaker Line 1351 (c:\vc10\r5xx\windows\hubframe.cpp)
		*/
		
		CDC memDC;
		memDC.CreateCompatibleDC(dc);
		HBITMAP pOldBitmap = (HBITMAP) SelectObject(memDC, dist);
		memDC.FillSolidRect(0, 0, nWidth, nHeight, clrBkColor);
		g_pImagesList->Draw(memDC, getImagePos(), CPoint(0, 0), ILD_NORMAL);
		SelectObject(memDC, pOldBitmap);
		return (HBITMAP)dist.Detach();
	}
	else
		return NULL;
}
//=======================================================================================
CAGEmotionSetup* CAGEmotionSetup::g_pEmotionsSetup = nullptr;
//=======================================================================================
void CAGEmotionSetup::destroyEmotionSetup()
{
	safe_delete(g_pEmotionsSetup);
}
//=======================================================================================
bool CAGEmotionSetup::reCreateEmotionSetup()
{
	destroyEmotionSetup();
	g_pEmotionsSetup = new CAGEmotionSetup;
	if (!g_pEmotionsSetup->Create())
	{
		safe_delete(g_pEmotionsSetup);
		SET_SETTING(EMOTICONS_FILE, "Disabled");
		dcassert(FALSE);
		return false;
	}
	return true;
}
//=======================================================================================
CAGEmotionSetup::~CAGEmotionSetup()
{
	cleanup();
}
void CAGEmotionSetup::cleanup()
{
	for_each(m_EmotionsArray.begin(), m_EmotionsArray.end(), DeleteFunction());
	m_EmotionsArray.clear();
	m_EmotionImages.Destroy();
	m_CountSelEmotions = 0;
	m_FilterEmotion.clear();
}
bool CAGEmotionSetup::LoadEmotion(const string& p_file_name)
{
	const string l_fileName = Util::getEmoPacksPath() + p_file_name + ".xml";
	if (!File::isExist(l_fileName))
		return true;
	try
	{
		SimpleXML xml;
		xml.fromXML(File(l_fileName, File::READ, File::OPEN).read());
		m_EmotionsArray.reserve(200);
		if (xml.findChild("Emoticons"))
		{
			xml.stepIn();
			string strEmotionText;
			string strEmotionBmpPath;
			string strEmotionGifPath;
			while (xml.findChild("Emoticon"))
			{
				strEmotionText = xml.getChildAttrib("PasteText");
				if (strEmotionText.empty())
					strEmotionText = xml.getChildAttrib("Expression");
				boost::algorithm::trim(strEmotionText);
				dcassert(!strEmotionText.empty());
				if (!strEmotionText.empty())
				{
//              dcdebug("CAGEmotionSetup::Create: emotion:[%s]\n", Text::fromT(strEmotionText).c_str());
//[!]PPA for lock bmp - только для вычисления висячих смайлов.
					/*
					try
					                    {
					                    File* l__f = new File (Util::getDataPath() + strEmotionBmpPath, File::READ, File::OPEN);
					                    }
					                    catch (const Exception& e)
					                    {
					                    }
					*/
					if (m_FilterEmotion.insert(strEmotionText).second == false) // [!] IRainman opt.
					{
						// Такой текст уже найден - но файла может и не быть?
						// Добавить альтернативный путь к смайлу на случай если не получится грузануть по первому пути?
//						LogManager::getInstance()->message("CAGEmotionSetup::Create: dup emotion:" + strEmotionText);
//						dcassert(0);
						continue;
					}
					strEmotionBmpPath = xml.getChildAttrib("Bitmap");
					strEmotionBmpPath = Util::getEmoPacksPath() + strEmotionBmpPath;
					strEmotionGifPath = xml.getChildAttrib("Gif");
					if (!strEmotionGifPath.empty())
						strEmotionGifPath = Util::getEmoPacksPath() + strEmotionGifPath;
						
					CAGEmotion* pEmotion = new CAGEmotion();
					pEmotion->registerEmotion(Text::toT(strEmotionText), strEmotionBmpPath, strEmotionGifPath);
					m_CountSelEmotions++;
					m_EmotionsArray.push_back(pEmotion);
				}
			}
			xml.stepOut();
		}
	}
	catch (const Exception& e)
	{
		dcdebug("CAGEmotionSetup::Create: %s\n", e.getError().c_str());
		return false;
	}
	return true;
}
bool CAGEmotionSetup::InitImages()
{
	CAGEmotion::setImageList(0);
	if (!m_EmotionImages.Create(18, 18, ILC_COLORDDB | ILC_MASK, 0, 1))
	{
		dcassert(FALSE);
		return false;
	}
	CAGEmotion::setImageList(&m_EmotionImages);
	return true;
}
bool CAGEmotionSetup::Create()
{
	setUseEmoticons(false);
	cleanup();
	const string l_CurentName = SETTING(EMOTICONS_FILE);
	if (l_CurentName == "Disabled")
		return true;
	LoadEmotion(l_CurentName);
	const int l_cnt_main = m_CountSelEmotions;
	const string l_base_smile = "FlylinkSmilesInternational";
	const string l_old_base_smile = "FlylinkSmiles";
	if (l_CurentName != l_base_smile)
		LoadEmotion(l_base_smile);
	if (l_CurentName != l_old_base_smile)
		LoadEmotion(l_old_base_smile);
	m_CountSelEmotions = l_cnt_main;
	
	// TODO - включить загрузку всех опционально?
	/*  const int l_cntMain = m_CountSelEmotions;
	    WIN32_FIND_DATA data;
	    HANDLE hFind = FindFirstFile(Text::toT(Util::getDataPath() + "EmoPacks\\*.xml").c_str(), &data);
	    if (hFind != INVALID_HANDLE_VALUE)
	    {
	        do
	        {
	            string l_name = Text::fromT(data.cFileName);
	            auto i = l_name.rfind('.');
	            l_name = l_name.substr(0, i);
	            if (l_CurentName != l_name) // текущую подгрузили выше
	            {
	                LoadEmotion(l_name);
	        }
	        }
	        while (FindNextFile(hFind, &data));
	        FindClose(hFind);
	    }
	    m_CountSelEmotions = l_cntMain;
	*/
	InitImages();
	setUseEmoticons(true);
	return true;
}

#endif  // IRAINMAN_INCLUDE_SMILE