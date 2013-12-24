/*
 * Copyright (C) 2011-2013 FlylinkDC++ Team http://flylinkdc.com/
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

#include "../client/BZUtils.h"
#include "../client/FilteredFile.h"
#include "../client/HashManager.h"
#include "../client/ShareManager.h"
#include "Resource.h"
#include "dclstGenDlg.h"
#include "WinUtil.h"
#include "../client/LogManager.h"


LRESULT DCLSTGenDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	if (_Dir == NULL || _usr == NULL)
		return TRUE;
		
		
	SetWindowText(CTSTRING(DCLSTGEN_TITLE));
	CenterWindow(GetParent());
	
	// fill in dialog bits
	SetDlgItemText(IDC_DCLSTGEN_INFOBORDER, CTSTRING(DCLSTGEN_INFOBORDER));
	SetDlgItemText(IDC_DCLSTGEN_INFO_SIZESTATIC, CTSTRING(DCLSTGEN_INFO_SIZESTATIC));
	SetDlgItemText(IDC_DCLSTGEN_INFO_FOLDERSSTATIC, CTSTRING(DCLSTGEN_INFO_FOLDERSSTATIC));
	SetDlgItemText(IDC_DCLSTGEN_INFO_FILESSTATIC, CTSTRING(DCLSTGEN_INFO_FILESSTATIC));
	SetDlgItemText(IDC_DCLSTGEN_NAMEBORDER, CTSTRING(DCLSTGEN_NAMEBORDER));
	SetDlgItemText(IDC_DCLSTGEN_NAMESTATIC, CTSTRING(MAGNET_DLG_FILE));
	SetDlgItemText(IDC_DCLSTGEN_SAVEAS, CTSTRING(DCLSTGEN_RENAMEAS));
	SetDlgItemText(IDC_DCLSTGEN_SHARE, CTSTRING(DCLSTGEN_SHARE));
	SetDlgItemText(IDC_DCLSTGEN_COPYMAGNET, CTSTRING(COPY_MAGNET));
	SetDlgItemText(IDCANCEL, CTSTRING(CANCEL));
	
	GetDlgItem(IDC_DCLSTGEN_SAVEAS).EnableWindow(FALSE);
	GetDlgItem(IDC_DCLSTGEN_SHARE).EnableWindow(FALSE);
	GetDlgItem(IDC_DCLSTGEN_COPYMAGNET).EnableWindow(FALSE);
	
	// for Custom Themes Bitmap
	_img_f.LoadFromResource(IDR_FLYLINK_PNG, _T("PNG"));
	GetDlgItem(IDC_STATIC).SendMessage(STM_SETIMAGE, IMAGE_BITMAP, LPARAM((HBITMAP)_img_f));
	// IDC_DCLSTGEN_PICT
	_img.LoadFromResource(IDR_DCLST, _T("PNG"));
	GetDlgItem(IDC_DCLSTGEN_PICT).SendMessage(STM_SETIMAGE, IMAGE_BITMAP, LPARAM((HBITMAP)_img));
	
	// _totalSizeDir = _Dir->getTotalSize();
	
	_filesCount = _Dir->getTotalFileCount();
	
	//::SendMessage(GetDlgItem(IDC_DCLSTGEN_PROGRESS), PBM_SETRANGE32, 0, 100);
	//::SendMessage(GetDlgItem(IDC_DCLSTGEN_PROGRESS), PBM_SETPOS, 5, 0L);
	_pBar.Attach(GetDlgItem(IDC_DCLSTGEN_PROGRESS));
	_pBar.SetRange32(0, 100);
	// _pBar.SetPos(5);
	_pBar.Detach();
	
	std::string fileName = _Dir->getName();
	if (fileName.empty())
		fileName = _usr->getLastNick();
	_mNameDCLST = getDCLSTName(fileName);
	SetDlgItemText(IDC_DCLSTGEN_NAME, Text::toT(_mNameDCLST).c_str());
	
	UpdateDialogItems();
	
	_isInProcess = true;
	start(0);
	
	
	return 0;
}


LRESULT DCLSTGenDlg::onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (_isInProcess)
	{
		_isCanceled = true;
		GetDlgItem(IDCANCEL).EnableWindow(FALSE);
		while (_isInProcess)
			::Sleep(100);
	}
	EndDialog(wID);
	return 0;
}

LRESULT DCLSTGenDlg::OnUpdateDCLSTWindow(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	UpdateDialogItems();
	return 0;
}

LRESULT DCLSTGenDlg::OnFinishedDCLSTWindow(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	UpdateDialogItems();
	
	SetDlgItemText(IDC_DCLSTGEN_MAGNET,  Text::toT(_strMagnet).c_str());
	
	GetDlgItem(IDC_DCLSTGEN_SAVEAS).EnableWindow(TRUE);
	GetDlgItem(IDC_DCLSTGEN_SHARE).EnableWindow(TRUE);
	GetDlgItem(IDC_DCLSTGEN_COPYMAGNET).EnableWindow(TRUE);
	return 0;
}

LRESULT DCLSTGenDlg::onCopyMagnet(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!_strMagnet.empty())
	{
		WinUtil::setClipboard(_strMagnet);
	}
	return 0;
}


void DCLSTGenDlg::UpdateDialogItems()
{
	SetDlgItemText(IDC_DCLST_GEN_INFOEDIT, Util::formatBytesW(_totalSize).c_str());
	SetDlgItemText(IDC_DCLSTGEN_INFO_FOLDERSEDIT, Util::toStringW(_totalFolders).c_str());
	SetDlgItemText(IDC_DCLSTGEN_INFO_FILESEDIT, Util::toStringW(_totalFiles).c_str());
	// ::SendMessage( GetDlgItem(IDC_DCLSTGEN_PROGRESS), PBM_SETPOS, (int) ( _totalSize * 100.0 )/_totalSizeDir, 0L);
	
	_pBar.Attach(GetDlgItem(IDC_DCLSTGEN_PROGRESS));
	_pBar.SetPos((int)(_totalFiles  / (double) _filesCount * 100.0));
	_pBar.Detach();
	if (_totalFiles == _filesCount)
		SetDlgItemText(IDCANCEL, CTSTRING(OK));
	// Invalidate();
}

string DCLSTGenDlg::getDCLSTName(const string& folderName)
{
	string folderDirName;
	if (!BOOLSETTING(DCLST_CREATE_IN_SAME_FOLDER) && !SETTING(DCLST_FOLDER_DIR).empty())
	{
		folderDirName = SETTING(DCLST_FOLDER_DIR);
	}
	else
	{
		folderDirName = SETTING(DOWNLOAD_DIRECTORY);
	}
	folderDirName += folderName;
	folderDirName += ".dcls";
	string retValue = File::isExist(folderDirName) ?  Util::getFilenameForRenaming(folderDirName) : folderDirName;
	return retValue;
}


int DCLSTGenDlg::run()
{
	// Start Generation
	// [-] _cs.lock(); [-] IRainman fix: no needs!
	if (_isCanceled)
	{
		_isInProcess = false;
		return 0;
	}
	MakeXMLCaption();
	
	if (_isCanceled)
	{
		_isInProcess = false;
		return 0;
	}
	WriteFolder(_Dir);
	
	if (_isCanceled)
	{
		_isInProcess = false;
		return 0;
	}
	MakeXMLEnd();
	
	if (_isCanceled)
	{
		_isInProcess = false;
		return 0;
	}
	PackAndSave();
	
	if (_isCanceled)
	{
		_isInProcess = false;
		return 0;
	}
	
	CalculateTTH();
	
	// [-] _cs.unlock(); [-] IRainman fix: no needs!
	
	PostMessage(WM_FINISHED, 0, 0);
	_isInProcess = false;
	return 0;
}

void DCLSTGenDlg::MakeXMLCaption()
{
	_xml = SimpleXML::utf8Header;
	_xml += "<FileListing Version=\"2\" CID=\"" + _usr->getCID().toBase32() + "\" Generator=\"DC++ " DCVERSIONSTRING "\"";
	if (BOOLSETTING(DCLST_INCLUDESELF))
		_xml += " IncludeSelf=\"1\"";
	_xml += ">\r\n";
	
}
void DCLSTGenDlg::MakeXMLEnd()
{
	_xml += "</FileListing>\r\n";
}

void DCLSTGenDlg::WriteFolder(DirectoryListing::Directory* dir)
{
	_totalFolders++;
	PostMessage(WM_UPDATE_WINDOW, 0, 0);
	::Sleep(1);
	string dirName = dir->getName();
	_xml += "<Directory Name=\"" + SimpleXML::escape(dirName, true) + "\">\r\n";
	
	if (_isCanceled) return;
	for (auto iDirIter = dir->directories.cbegin(); iDirIter != dir->directories.cend(); ++iDirIter)
	{
		if (*iDirIter)
		{
			WriteFolder(*iDirIter);
		}
		if (_isCanceled) return;
	}
	
	if (_isCanceled) return;
	
	for (auto iFileIter = dir->files.cbegin(); iFileIter != dir->files.cend(); ++iFileIter)
	{
		if (*iFileIter)
		{
			WriteFile(*iFileIter);
		}
		if (_isCanceled) return;
	}
	
	_xml += "</Directory>\r\n";
	
}
// TODO fix copy=past: move to ShareManager!
void DCLSTGenDlg::WriteFile(DirectoryListing::File* file)
{
	if (_isCanceled) return;
	_totalFiles++;
	_totalSize += file->getSize();
	string fileName = file->getName();
	_xml += "<File Name=\"" + SimpleXML::escape(fileName, true) + "\" Size=\"" + Util::toString(file->getSize()) + "\" TTH=\"" + file->getTTH().toBase32() + '\"';
	
	if (file->getTS())
	{
		_xml += " TS=\"" + Util::toString(file->getTS()) + '\"';
	}
	if (file->m_media)
	{
		if (file->m_media->m_bitrate)
		{
			_xml += " BR=\"" + Util::toString(file->m_media->m_bitrate) + '\"';
		}
		if (file->m_media->m_mediaX && file->m_media->m_mediaY)
		{
			_xml += " WH=\"" + file->m_media->getXY() + '\"';
		}
		if (!file->m_media->m_audio.empty())
		{
			string maudio = file->m_media->m_audio;
			_xml += " MA=\"" + SimpleXML::escape(maudio, true) + '\"';
		}
		if (!file->m_media->m_video.empty())
		{
			string mvideo = file->m_media->m_video;
			_xml += " MV=\"" + SimpleXML::escape(mvideo, true) + '\"';
		}
	}
	_xml += "/>\r\n";
	if (_isCanceled)
	{
		return;
	}
	PostMessage(WM_UPDATE_WINDOW, 0, 0);
	::Sleep(1);
}

size_t DCLSTGenDlg::PackAndSave()
{
	if (_isCanceled) return 0;
	size_t outSize = 0;
	try
	{
		unique_ptr<OutputStream> outFilePtr(new File(_mNameDCLST, File::WRITE, File::TRUNCATE | File::CREATE, false));
		FilteredOutputStream<BZFilter, false> outFile(outFilePtr.get());
		outSize += outFile.write(_xml.c_str(), _xml.size());
		outSize += outFile.flush();
	}
	catch (const FileException& ex)
	{
		LogManager::getInstance()->message("DCLSTGenerator::File error = " + ex.getError() + " File = " + _mNameDCLST); // [!] IRainman fix.
		MessageBox(CTSTRING(DCLSTGEN_METAFILECANNOTBECREATED), CTSTRING(DCLSTGEN_TITLE), MB_OK | MB_ICONERROR);
	}
	
	return outSize;
}

void DCLSTGenDlg::CalculateTTH()
{
	const size_t c_size_buf = 1024 * 1024; // [!] IRainman fix.
	if (Util::getTTH_MD5(_mNameDCLST, c_size_buf, &_tth/*, &l_md5*/))
	{
		const string l_TTH_str = _tth.get()->getRoot().toBase32();
		
		_strMagnet = "magnet:?xt=urn:tree:tiger:" + l_TTH_str +
		             "&xl=" + Util::toString(_tth.get()->getFileSize()) + "&dn=" + Util::encodeURI(Util::getFileName(_mNameDCLST)) + "&dl=" + Util::toString(_totalSize);
	}
}

LRESULT
DCLSTGenDlg::onShareThis(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!_strMagnet.empty() && !_tth == NULL)
	{
		const string strPath = _mNameDCLST;
		if (ShareManager::getInstance()->isFileInSharedDirectoryL(strPath))
		{
			HashManager::getInstance()->addTree(strPath, File::getTimeStamp(strPath), *(_tth.get()), -1);
			if (ShareManager::getInstance()->isTTHShared(_tth.get()->getRoot()))
			{
				MessageBox(CTSTRING(DCLSTGEN_METAFILEREADY), CTSTRING(DCLSTGEN_TITLE), MB_OK | MB_ICONINFORMATION);
			}
		}
		else
		{
			MessageBox(CTSTRING(DCLSTGEN_METAFILECANNOTBESHARED), CTSTRING(DCLSTGEN_TITLE), MB_OK | MB_ICONERROR);
		}
	}
	
	return 0;
}

LRESULT
DCLSTGenDlg::onSaveAS(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	// GetNew _mNameDCLST
	tstring targetOld = Text::toT(_mNameDCLST);
	tstring target = targetOld;
	static const TCHAR defaultExt[] = L"dcls";
	if (WinUtil::browseFile(target, *this, true, Util::emptyStringT, FILE_LIST_TYPE_T, defaultExt)) // TODO translate
	{
		try
		{
			File::renameFile(targetOld, target);
			_mNameDCLST = Text::fromT(target);
			
			const string l_TTH_str = _tth.get()->getRoot().toBase32();
			
			_strMagnet = "magnet:?xt=urn:tree:tiger:" + l_TTH_str +
			             "&xl=" + Util::toString(_tth.get()->getFileSize()) + "&dn=" + Util::encodeURI(Util::getFileName(_mNameDCLST)) + "&dl=" + Util::toString(_totalSize);
			             
			SetDlgItemText(IDC_DCLSTGEN_MAGNET,  Text::toT(_strMagnet).c_str());
			SetDlgItemText(IDC_DCLSTGEN_NAME, target.c_str());
		}
		catch (const FileException& /*ex*/)
		{
			MessageBox(CTSTRING(DCLSTGEN_METAFILECANNOTMOVED), CTSTRING(DCLSTGEN_TITLE), MB_OK | MB_ICONERROR);
		}
		
	}
	return 0;
}

/**
* @file
* $Id: dclstGenDlg.cpp 8025 2011-08-25 22:03:07Z a.rainman $
*/