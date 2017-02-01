/*
 * Copyright (C) 2011-2017 FlylinkDC++ Team http://flylinkdc.com
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

#ifndef _AUTO_UPDATE_H_
#define _AUTO_UPDATE_H_

#pragma once

#include "../client/SettingsManager.h"
#include "../client/TimerManager.h"
#include "../XMLParser/xmlParser.h"

// [+] SSA

class AutoUpdateFile
{
	public:
		enum OpSystem
		{
			xAll = 0,
			x86_legacy,
			x86_64,
#ifdef IRAINMAN_AUTOUPDATE_ARCH_DIFFERENCE
			x86_SSE2,
			x86_AVX,
			x86_64_AVX,
#endif
			OsUnknown
		};
		enum Packers
		{
			Unpacked = 0,
			BZip2,
			Zip,
			PackUnknown
		};
		string m_sName;
		string m_sPath;
		OpSystem m_sys;
		Packers  m_packer;
		int64_t  m_size;
		int64_t  m_packedSize;
		string m_sTTH;
		string m_sDownloadURL;
};

typedef std::vector<AutoUpdateFile> AutoUpdateFiles;

class AutoUpdateModule
{
	public:
		string m_sName;
		SettingsManager::IntSetting    m_iSetting;
		AutoUpdateFiles m_Files;
};

typedef std::vector<AutoUpdateModule> AutoUpdateModules;

class AutoUpdateObject
{
	public:
		string m_sVersion;
		string m_sUpdateDate;
		AutoUpdateModules m_Modules;
		AutoUpdateModule m_updater;
		string m_update_xml;
		bool checkSignXML(const string& p_url_sign) const;
};

enum AutoUpdateTasks
{
	START_UPDATE,
};

class AutoUpdateGUIMethod
{
	public:
		virtual ~AutoUpdateGUIMethod() { }
		virtual UINT ShowDialogUpdate(const std::string& message, const std::string& rtfMessage, const AutoUpdateFiles& fileList) = 0;
		virtual void NewVerisonEvent(const std::string& p_new_version) = 0;
};

class AutoUpdate :
	public Singleton<AutoUpdate>,
	public BackgroundTaskExecuter<AutoUpdateTasks, 10000>,
	private TimerManagerListener
{
		friend class AutoUpdateObject;
	public:
		void initialize(HWND p_mainFrameHWND, AutoUpdateGUIMethod* p_guiDelegate);
		void startUpdateManually();
		bool startupUpdate();
		bool isUpdate() const
		{
			return m_isUpdate;
		}
		bool isUpdateStarted() const
		{
			return m_isUpdateStarted != 0;
		}
		void shutdownAndUpdate();
		
		static bool getExitOnUpdate()
		{
			return g_exitOnUpdate;
		}
		
		enum UpdateResult
		{
			UPDATE_NOW = 0,
			UPDATE_IGNORE,
			UPDATE_CANCEL, // next time
		};
		
	private:
		int   m_isUpdateStarted;
		bool  m_isUpdate;
		HWND m_mainFrameHWND;
		AutoUpdateGUIMethod* m_guiDelegate;
		bool m_manualUpdate;
		static bool g_exitOnUpdate;
		tstring m_updateFolder;
		
		friend class Singleton<AutoUpdate>;
		
		explicit AutoUpdate() : m_isUpdateStarted(0), m_isUpdate(false), m_manualUpdate(false), m_mainFrameHWND(nullptr), m_guiDelegate(nullptr)
		{
			g_exitOnUpdate = false;
		}
		
		virtual ~AutoUpdate()
		{
			waitShutdown();
		}
		
		static void fail(const string& p_error);
		static void message(const string& p_message);
		void execute(const AutoUpdateTasks& p_task);
		
		virtual void on(TimerManagerListener::Hour, uint64_t aTick) noexcept override;
		
		void startUpdateThisThread();
		
		void runFlyUpdate();
		
		static SettingsManager::IntSetting getSettingByTitle(const string& wTitle);
		static bool needUpdateFile(const AutoUpdateFile& p_file, const string& p_outputFolder);
		static bool prepareFile(const AutoUpdateFile& p_file, const string& p_tempFolder);
		static string getTempFolderForUpdate(const string &version);
		static bool verifyUpdate(byte* signData, size_t signDataSize, const string& data, size_t dataSize);
		
		static const string UPDATE_FILE_DOWNLOAD();
		static const string UPDATE_SIGN_FILE_DOWNLOAD();
		static const string UPDATE_DESCRIPTION();
		static string getAUTOUPDATE_SERVER_URL();
		static AutoUpdateFile parseNode(const XMLParser::XMLNode& Node);
		int64_t checkFilesToNeedsUpdate(AutoUpdateFiles& p_files4Update,
		                                AutoUpdateFiles& p_files4Description,
		                                const AutoUpdateModules& p_modules,
		                                const string& p_path);
		bool preparingFiles(const AutoUpdateFiles& p_files, const string& p_path, string& p_errorFileName);
		string  getUpdateFilesList(const string&  p_componentName,
		                           string  p_serverUrl, // —сылку не делать!
		                           const char*   p_rootNode,
		                           const string& p_file,
		                           const string& p_descr,
		                           unique_ptr<AutoUpdateObject>& p_autoUpdateObject,
		                           string& p_base_update_url);
};

#endif // _AUTO_UPDATE_H_