/*
 * Copyright (C) 2001-2013 Jacek Sieka, arnetheduck on gmail point com
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

#if !defined(ABOUT_DLG_H)
#define ABOUT_DLG_H

#include <boost/version.hpp>
#include "../bzip2/bzlib_private.h"
#include "../miniupnpc/miniupnpc.h"
//#include "../XMLParser/XMLParser.h"
#include "HIconWrapper.h"
#include "wtl_flylinkdc.h"
#include "leveldb/db.h"
//#include "../openssl/include/openssl/crypto.h"
//#include "MediaInfo/MediaInfo_Config.h"

/*
static const TCHAR g_donation[] =
    _T("Яндекс.Деньги:\r\n")
    _T(" 4100197690217\r\n")
    _T("WebMoney:\r\n")
    _T(" R324721262364\r\n")
    _T(" Z187845045394\r\n")
    _T(" U154459391462\r\n")
    ;
*/

class AboutDlg : public CDialogImpl<AboutDlg>
#ifdef _DEBUG
	, boost::noncopyable , virtual NonDerivable<AboutDlg> // [+] IRainman fix.
#endif
{
	public:
		enum { IDD = IDD_ABOUTBOX };
		
		AboutDlg() { }
		~AboutDlg() {}
		
		BEGIN_MSG_MAP(AboutDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDC_LINK, onLink)
		COMMAND_ID_HANDLER(IDC_SUPPORT_MAIL, onMailLink)
		COMMAND_ID_HANDLER(IDC_PVS_STUDIO, onPVSStudioLink)
		COMMAND_ID_HANDLER(IDC_LINK_BLOG, onBlogLink)
		END_MSG_MAP()
		
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			SetDlgItemText(IDC_VERSION, T_VERSIONSTRING); // [~] (Sergey Shushkanov)
			
			static const TCHAR l_thanks[] =
			    _T("Programming, patches:\r\n")
			    _T("PPA <pavel.pimenov@gmail.com>\r\nBrain RIPper\r\nSergey Stolper\r\nIRainman\r\nNightOrion\r\nSCALOlaz\r\nreg <entry.reg@gmail.com>\r\nMikhail Korbakov <mike.korbakov@gmail.com>\r\n")
			    
			    _T("\r\nPortal Browser, crash-dump, x64 platform, GDI+ animation:\r\n")
			    _T("Brain RIPper\r\n")
			    
			    _T("\r\nFlylinkDC++ HE, DC++ core NG:\r\n")
			    _T("IRainman\r\n")
			    
			    _T("\r\nPatches:\r\n")
			    _T("SMT\r\nBugMaster (ApexDC++ mod 2)\r\nKlirik\r\nnecros\r\nDecker\r\nDrakon\r\nmt2006 wine(Linux)\r\n")
			    _T("WhiteD\r\n ZagZag\r\nNSLQQQ\r\n")
			    
			    _T("\r\nWeb-Interface, PHP:\r\n")
			    _T("IRainman\r\n Paul\r\n Iceberg\r\n M.S.A\r\n SkazochNik\r\n")
			    
			    _T("\r\nRSS, CustomMenu:\r\n")
			    _T("Sergey Stolper\r\n")
			    
			    _T("\r\nCustomlocations database:\r\n")
			    _T("lazybadger\r\n")
			    
			    _T("\r\nDesign:\r\n")
			    _T("moltiplico\r\n KS!ON\r\n Dodge-X (баннер)\r\n Сергей Шушканов\r\n")
			    
			    _T("\r\nDocumentation:\r\n")
			    _T("Berik (модемы)\r\nM.S.A.\r\n")
			    
			    _T("\r\nBugreports:\r\n")
			    _T("Squork\r\n joedm\r\n")
			    
			    _T("\r\nTranslators:\r\n")
			    _T("Nikitos\r\n Tankafett\r\n R00T_ADMIN\r\n i.Kliok\r\n hat3k\r\n VladV\r\n Xcell\r\n BlackRiderUA\r\n Havokdan\r\n")
			    
			    _T("\r\nBeta-testers:\r\n")
			    _T(" Pafuntik\r\n kotik\r\n Tirael\r\n MoonRainbow\r\n BlackRiderUA\r\n Black-code\r\n JhaoDa\r\n Papochka\r\n R00T_ADMIN\r\n Nikitos\r\n -_SoRuS_-\r\n Леонид Жилин lss.jilin@gmail.com\r\n ShadoWx");
			CEdit ctrlThanks(GetDlgItem(IDC_THANKS));
			ctrlThanks.FmtLines(TRUE);
			ctrlThanks.AppendText(l_thanks, TRUE);
			ctrlThanks.Detach();
			SetDlgItemText(IDC_THANKS_STATIC, (TSTRING(ABOUT_PARTICIPANTS) + L':').c_str());
			SetDlgItemText(IDC_STATISTICS_STATIC, (TSTRING(ABOUT_STATISTICS) + L':').c_str());
			SetDlgItemText(IDC_LINKS_STATIC, (TSTRING(ABOUT_LINKS) + L':').c_str());
			SetDlgItemText(IDC_SIDEPRO_STATIC, (TSTRING(ABOUT_SIDE_PROJECTS) + L':').c_str());
			static const TCHAR l_Party_Software[] =
			    _T("sqlite ") _T(SQLITE_VERSION) _T("\r\n")
			    _T("LevelDB ") _T(LEVELDB_VER) _T("\r\n")
			    _T("boost ") _T(BOOST_LIB_VERSION) _T("\r\n")
			    _T("bzip ") _T(BZ_VERSION) _T("\r\n")
			    _T("zlib ") _T(ZLIB_VERSION) _T("\r\n")
			    _T("jsoncpp 0.5.0\r\n")
			    _T("miniupnpc ") _T(MINIUPNPC_VERSION) _T("\r\n")
			    _T("ZenLib 0.4.29\r\n")
			    _T("OpenSSL 1.0.1e\r\n") //SSLeay_version(SSLEAY_VERSION)
			    _T("MediaInfoLib 0.7.67\r\n")//MediaInfoLib::MediaInfo_Version _T("\r\n")
			    _T("WTL 8.1\r\n")
			    _T("XMLParser 2.43\r\n") //XMLParser::XMLNode::getVersion()
			    _T("InnoSetup 5.5.3");
			CEdit ctrlPartySoftware(GetDlgItem(IDC_THIRD_PARTY_SOFTWARE));
			ctrlPartySoftware.FmtLines(TRUE);
			ctrlPartySoftware.AppendText(l_Party_Software, FALSE);
			ctrlPartySoftware.Detach();
			
			::SetWindowText(GetDlgItem(IDC_UPDATE_VERSION_CURRENT_LBL), (TSTRING(CURRENT_VERSION) + _T(":")).c_str()); //[+] (Sergey Shushkanov)
			
//[-]PPA    SetDlgItemText(IDC_TTH, WinUtil::tth.c_str());
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
			CFlylinkDBManager::getInstance()->load_global_ratio();
			SetDlgItemText(IDC_LATEST, CTSTRING(DOWNLOADING));
			SetDlgItemText(IDC_TOTAL_UPLOAD, (TSTRING(UPLOADED) + _T(": ") +
			                                  Text::toT(Util::formatBytes(CFlylinkDBManager::getInstance()->m_global_ratio.m_upload))).c_str());
			SetDlgItemText(IDC_TOTAL_DOWNLOAD, (TSTRING(DOWNLOADED) + _T(": ") +
			                                    Text::toT(Util::formatBytes(CFlylinkDBManager::getInstance()->m_global_ratio.m_download))).c_str());
			                                    
			SetDlgItemText(IDC_RATIO, (TSTRING(RATING) + _T(": ") + (CFlylinkDBManager::getInstance()->get_ratioW())).c_str());
#endif
			
			char l_full_version[64];
			snprintf(l_full_version, _countof(l_full_version), "%d", _MSC_FULL_VER);
			
			SetDlgItemText(IDC_LINK, CTSTRING(MENU_HOMEPAGE));
			m_url.init(GetDlgItem(IDC_LINK), _T(HOMEPAGE));
			
			SetDlgItemText(IDC_LINK_BLOG, CTSTRING(NEWS_BLOG));
			m_url_blog.init(GetDlgItem(IDC_LINK_BLOG), _T(HOMEPAGE_BLOG));
			
			m_Mail.init(GetDlgItem(IDC_SUPPORT_MAIL), TSTRING(REPORT_BUGS));
			
			SetDlgItemText(IDC_PVS_STUDIO, TSTRING(PVS_STUDIO_URL).c_str());
			m_PVSStudio.init(GetDlgItem(IDC_PVS_STUDIO), TSTRING(PVS_STUDIO_URL));
			
			//CenterWindow(GetParent());
			return TRUE;
		}
		
		LRESULT onMailLink(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			WinUtil::openLink(_T("mailto:pavel.pimenov@gmail.com"));
			return 0;
		}
		LRESULT onPVSStudioLink(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			WinUtil::openLink(_T("http://www.viva64.com"));
			return 0;
		}
		LRESULT onLink(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			WinUtil::openLink(_T(HOMEPAGE));
			return 0;
		}
		LRESULT onBlogLink(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) //-V524
		{
			WinUtil::openLink(_T(HOMEPAGE_BLOG));
			return 0;
		}
		
	private:
		CFlyHyperLink m_url;
		CFlyHyperLink m_url_blog;
		CFlyHyperLink m_Mail;
		CFlyHyperLink m_PVSStudio;
};

#endif // !defined(ABOUT_DLG_H)