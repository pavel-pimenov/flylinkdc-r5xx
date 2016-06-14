/*
 * Copyright (C) 2011-2016 FlylinkDC++ Team http://flylinkdc.com/
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

#include "stdinc.h"
#include "AutoUpdate.h"
#include <Shellapi.h>
#include "../client/Util.h"
#include "../client/ResourceManager.h"
#include "../client/LogManager.h"
#include "../client/CompatibilityManager.h"
#include "../client/ClientManager.h"
#include "../client/file.h"
#include "fuSearch.h"
#include "ZUtils.h"
#include "BZUtils.h"
#include <openssl/pem.h>
#include "FlylinkDCKey.h"
#include "InetDownloaderReporter.h"
#include "flyServer.h"

static const string g_dev_error = "\r\nPlease send a text or a screenshot of the error to developers ppa74@ya.ru";
static const wstring UPDATE_FILE_NAME = L"flylink.upd";

static const string UPDATE_RELEASE_URL = "http://update.fly-server.ru/update/5xx/release";
static const string UPDATE_BETA_URL = "http://update.fly-server.ru/update/5xx/beta";

static const string UPDATE_FILE_DOWNLOAD_B = "Update5_beta.xml";
static const string UPDATE_SIGN_FILE_DOWNLOAD_B = "Update5_beta.sign";
static const string UPDATE_DESCRIPTION_B = "Update5_beta.rtf";

static const string UPDATE_FILE_DOWNLOAD_R = "Update5.xml";
static const string UPDATE_SIGN_FILE_DOWNLOAD_R = "Update5.sign";
static const string UPDATE_DESCRIPTION_R = "Update5.rtf";

#ifdef IRAINMAN_AUTOUPDATE_ALL_USERS_DATA
static const string UPDATE_AU_URL = "http://update.fly-server.ru/update/alluser";
static const string UPDATE_UPDATE_FILE = "UpdateAU.xml";
static const string UPDATE_SIGN_FILE = "UpdateAU.sign";
static const string UPDATE_DESCRIPTION_FILE = "UpdateAU.rtf";
#endif

bool AutoUpdate::g_exitOnUpdate = false;
void AutoUpdate::initialize(HWND p_mainFrameHWND, AutoUpdateGUIMethod* p_guiDelegate)
{
	m_mainFrameHWND = p_mainFrameHWND;
	m_guiDelegate = p_guiDelegate;
	TimerManager::getInstance()->addListener(this);
#ifndef AUTOUPDATE_NOT_DISABLE
	if (BOOLSETTING(AUTOUPDATE_ENABLE) && BOOLSETTING(AUTOUPDATE_RUNONSTARTUP))
#endif
	{
		addTask(START_UPDATE);
	}
}
void AutoUpdate::shutdownAndUpdate()
{
	m_guiDelegate = nullptr; // https://drdump.com/Problem.aspx?ProblemID=106436
	forceStop();
	TimerManager::getInstance()->removeListener(this);
	runFlyUpdate();
}

void AutoUpdate::on(TimerManagerListener::Hour, uint64_t tick) noexcept
{
	if (isUpdateStarted())
		return;
		
#ifndef AUTOUPDATE_NOT_DISABLE
	if (BOOLSETTING(AUTOUPDATE_ENABLE) && BOOLSETTING(AUTOUPDATE_STARTATTIME))
#endif
	{
		// Check Current Time
		time_t currentTime = GET_TIME();
		tm*  currentLocalTime = localtime(&currentTime);
		if (currentLocalTime)
		{
			const int l_settings = SETTING(AUTOUPDATE_TIME);
			if (
#ifdef HOURLY_CHECK_UPDATE
			    24 == l_settings ||
#endif
			    currentLocalTime->tm_hour == l_settings)
			{
				addTask(START_UPDATE);
			}
		}
	}
}

void AutoUpdate::startUpdateManually()
{
	if (isUpdateStarted())
		return;
		
	m_manualUpdate = true;
	
	// [!] SSA - set the ignore update list as empty if manual update started.
	SET_SETTING(AUTOUPDATE_IGNORE_VERSION, Util::emptyString);
	
	addTask(START_UPDATE);
}

void AutoUpdate::execute(const AutoUpdateTasks& p_task)
{
#ifdef _DEBUG
//	return;
#endif
	dcassert(p_task == START_UPDATE);
	startUpdateThisThread();
}
bool AutoUpdate::preparingFiles(const AutoUpdateFiles& p_files, const string& p_path, string& p_errorFileName)
{
	for (size_t i = 0; i < p_files.size(); i++)
	{
		message(STRING(LOADING_FILE) + ' ' + p_files[i].m_sName);
		if (prepareFile(p_files[i], p_path))
		{
			message(p_files[i].m_sName + ' ' + STRING(AUTOUPDATE_SUCCESS));
		}
		else
		{
			p_errorFileName = p_files[i].m_sDownloadURL;
			break;
		}
	}
	return p_errorFileName.empty();
};
int64_t AutoUpdate::checkFilesToNeedsUpdate(AutoUpdateFiles& p_files4Update, AutoUpdateFiles& p_files4Description, const AutoUpdateModules& p_modules, const string& p_path)
{
	int64_t l_local_totalSize = 0;
	for (auto i = p_modules.cbegin(); i != p_modules.cend(); ++i)
	{
		if (i->m_iSetting != SettingsManager::INT_LAST &&
		        SettingsManager::getBool(i->m_iSetting, true))
		{
			for (auto j = i->m_Files.cbegin(); j != i->m_Files.cend(); ++j)
			{
				const AutoUpdateFile& l_file = *j;
				if (needUpdateFile(l_file, p_path))
				{
					p_files4Update.push_back(l_file);
					p_files4Description.push_back(l_file);
					l_local_totalSize += l_file.m_packedSize;
				}
			}
		}
	}
	return l_local_totalSize;
}
string AutoUpdate::getUpdateFilesList(const string& p_componentName,
                                      string  p_serverUrl,
                                      const char*   p_rootNode,
                                      const string& p_file,
                                      const string& p_descr,
                                      unique_ptr<AutoUpdateObject>& p_autoUpdateObject,
                                      string& p_base_update_url)
{
	AppendUriSeparator(p_serverUrl);
	p_base_update_url = p_serverUrl;
	const string l_serverUpdateFile = p_serverUrl + p_file;
	const string l_log_url_info = " (" + p_componentName + ')' + " ( URL:" + l_serverUpdateFile + ')';
	// 60 sec - http://code.google.com/p/flylinkdc/issues/detail?id=1403
	size_t l_dataSize =  Util::getDataFromInetSafe(false, l_serverUpdateFile, p_autoUpdateObject->m_update_xml, 60000);
	if (l_dataSize)
	{
		message(STRING(AUTOUPDATE_DOWNLOAD_SUCCESS) + l_log_url_info);
		XMLParser::XMLResults xRes;
		// Try to parse data
		XMLParser::XMLNode xRootNode = XMLParser::XMLNode::parseString(p_autoUpdateObject->m_update_xml.c_str(), 0, &xRes);
		if (xRes.error == XMLParser::eXMLErrorNone)
		{
			XMLParser::XMLNode update5Node = xRootNode.getChildNode(p_rootNode);
			dcassert(!update5Node.isEmpty());
			if (!update5Node.isEmpty())
			{
				p_autoUpdateObject->m_sVersion = update5Node.getAttributeOrDefault("Version");
				p_autoUpdateObject->m_sUpdateDate = update5Node.getAttributeOrDefault("UpdateDate");
				// Let's asks for updater....
				{
					int i = 0;
					XMLParser::XMLNode uNode = update5Node.getChildNode("Updater", &i);
					if (!uNode.isEmpty())
					{
						int j = 0;
						XMLParser::XMLNode fileNode = uNode.getChildNode("File", &j);
						while (!fileNode.isEmpty())
						{
							p_autoUpdateObject->m_updater.m_Files.push_back(AutoUpdate::parseNode(fileNode));
							fileNode = uNode.getChildNode("File", &j);
						}
					}
				}
				{
					int i = 0;
					XMLParser::XMLNode moduleNode = update5Node.getChildNode("Module", &i);
					while (!moduleNode.isEmpty())
					{
						AutoUpdateModule module;
						module.m_sName = moduleNode.getAttributeOrDefault("Title");
						module.m_iSetting = getSettingByTitle(module.m_sName);
						
						int j = 0;
						XMLParser::XMLNode fileNode = moduleNode.getChildNode("File", &j);
						while (!fileNode.isEmpty())
						{
							module.m_Files.push_back(parseNode(fileNode));
							fileNode = moduleNode.getChildNode("File", &j);
						}
						p_autoUpdateObject->m_Modules.push_back(module);
						
						moduleNode = update5Node.getChildNode("Module", &i);
					}
				}
			}
			else
			{
				const string l_error = "update5Node.isEmpty() - send error - ppa74@ya.ru (p_rootNode = [" + string(p_rootNode) + "]";
				CFlyServerJSON::pushError(63, l_error);
				::MessageBox(m_mainFrameHWND, Text::toT(l_error).c_str(), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_OK | MB_ICONERROR);
			}
		}
	}
	return p_serverUrl + p_descr;
}
bool AutoUpdateObject::checkSignXML(const string& p_url_sign) const
{
	std::vector<byte> l_signData;
	CFlyHTTPDownloader l_http_downloader;
	const size_t l_signDataSize = l_http_downloader.getBinaryDataFromInet(p_url_sign, l_signData); // update*.sign fize == 128
	return l_signDataSize != 0 && AutoUpdate::verifyUpdate(l_signData.data(), l_signDataSize, m_update_xml, m_update_xml.length());
}
void AutoUpdate::startUpdateThisThread()
{
	if (ClientManager::isShutdown())
		return;
	bool l_is_invalid_sign_update = false;
	//for (unsigned l_update_index = 0; l_update_index < 3; ++l_update_index)
	{
		CFlySafeGuard<int> l_satrt(m_isUpdateStarted);
		InetDownloadReporter::getInstance()->ReportResultWait(0);
		unique_ptr<AutoUpdateObject> l_autoUpdateObject(new AutoUpdateObject());
#ifdef IRAINMAN_AUTOUPDATE_ALL_USERS_DATA
		unique_ptr<AutoUpdateObject> l_autoUpdateObjectAU(new AutoUpdateObject());
#endif
		const string l_serverURL = getAUTOUPDATE_SERVER_URL();
		const string l_serverURL_AU = UPDATE_AU_URL;
		// TODO - в случае ошибки сделать итерацию и пройтись по зеркалам
		string l_base_update_url;
		string l_base_updateAU_url;
		if (Util::isHttpLink(l_serverURL))
		{
			const string programUpdateDescription = getUpdateFilesList(STRING(PROGRAM_FILES), l_serverURL, "Update5",
			                                                           UPDATE_FILE_DOWNLOAD(), UPDATE_DESCRIPTION(), l_autoUpdateObject,
			                                                           l_base_update_url);
#ifdef IRAINMAN_AUTOUPDATE_ALL_USERS_DATA
			// TODO - результат не юзается const string basesUpdateDescription =
			getUpdateFilesList(STRING(PROGRAM_DATA), l_serverURL_AU, "UpdateAU",
			                   UPDATE_UPDATE_FILE, UPDATE_DESCRIPTION_FILE, l_autoUpdateObjectAU,
			                   l_base_updateAU_url);
#endif
			if (!l_autoUpdateObject->m_Modules.empty()
#ifdef IRAINMAN_AUTOUPDATE_ALL_USERS_DATA
			        || !l_autoUpdateObjectAU->m_Modules.empty()
#endif
			   )
			{
				// Process with Object
				// check what files to update....
				AutoUpdateFiles l_files4Update;
				AutoUpdateFiles l_files4UpdateInSettings;
#ifdef IRAINMAN_AUTOUPDATE_ALL_USERS_DATA
				AutoUpdateFiles l_files4UpdateInAllUsersSettings;
				AutoUpdateFiles l_files4Description;
#endif
				int64_t l_totalSize = 0;
				if (SETTING(AUTOUPDATE_IGNORE_VERSION) != l_autoUpdateObject->m_sVersion)
				{
					l_totalSize += checkFilesToNeedsUpdate(l_files4Update, l_files4Description, l_autoUpdateObject->m_Modules, Util::getExePath());
#ifdef IRAINMAN_AUTOUPDATE_ALL_USERS_DATA
					l_totalSize += checkFilesToNeedsUpdate(l_files4UpdateInAllUsersSettings, l_files4Description, l_autoUpdateObjectAU->m_Modules, Util::getConfigPath(
#ifndef USE_SETTINGS_PATH_TO_UPDATA_DATA
					                                           true
#endif
					                                       ));
#endif
				}
				else
				{
					m_isUpdate = false;
					message(STRING(AUTOUPDATE_IGNORED));
				}
				bool l_needsToStartUpdate = !l_files4Update.empty();
#ifdef IRAINMAN_AUTOUPDATE_ALL_USERS_DATA
				const bool needsUpdateBases = !l_files4UpdateInAllUsersSettings.empty();
#endif
				if (l_needsToStartUpdate
#ifdef IRAINMAN_AUTOUPDATE_ALL_USERS_DATA
				        || needsUpdateBases
#endif
				   )
				{
					m_isUpdate = false;
					// Check if Need to Update UpdateUpdater
					for (size_t j = 0; j < l_autoUpdateObject->m_updater.m_Files.size(); j++)
					{
						const AutoUpdateFile& file = l_autoUpdateObject->m_updater.m_Files[j];
						if (needUpdateFile(file, Util::getConfigPath()))
						{
							l_files4UpdateInSettings.push_back(file);
							l_files4Description.push_back(file);
							l_totalSize += file.m_packedSize;
						}
					}
					//
					if (l_totalSize) // Есть кандидаты для обновления?
					{
						string l_check_file_url = l_base_update_url + UPDATE_SIGN_FILE_DOWNLOAD();
						string l_check_message;
						bool l_check_result = l_autoUpdateObject->checkSignXML(l_check_file_url);
						if (l_check_result == false)
						{
							l_check_message = STRING(AUTOUPDATE_ERROR_VERIF) + " url: " + l_check_file_url;
							l_is_invalid_sign_update = true;
						}
#ifdef IRAINMAN_AUTOUPDATE_ALL_USERS_DATA
						l_check_file_url = l_base_updateAU_url + UPDATE_SIGN_FILE;
						l_check_result = l_autoUpdateObjectAU->checkSignXML(l_check_file_url);
						if (l_check_result == false)
						{
							l_check_message += "\r\n";
							l_check_message += STRING(AUTOUPDATE_ERROR_VERIF) + " url: " + l_check_file_url;
							l_is_invalid_sign_update = false;
						}
#endif
						if (!l_check_message.empty())
						{
							m_isUpdate = false;
							m_manualUpdate = false;
							const string l_error = STRING(AUTOUPDATE_ERROR_VERIF) + l_check_message;
							fail(l_error);
							CFlyServerJSON::pushError(63, l_error);
							::MessageBox(m_mainFrameHWND, Text::toT(l_error).c_str(), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_OK | MB_ICONERROR);
							return;
							//continue;
						}
					}
					// Create TempFolder
					static string g_tempFolder = getTempFolderForUpdate(l_autoUpdateObject->m_sVersion);
					
					// Start Update (show info if need?)
					bool needToUpdate = true;
					string l_message = STRING(AUTOUPDATE_DOWNLOAD_START1);
					l_message += ' ' + l_autoUpdateObject->m_sVersion;
					l_message += " (" + l_autoUpdateObject->m_sUpdateDate + ") ";
					l_message += STRING(AUTOUPDATE_DOWNLOAD_START2);
					l_message += ' ' + Util::formatBytes(l_totalSize) + " (";
					l_message += Util::toString(l_files4Update.size() + l_files4UpdateInSettings.size()
#ifdef IRAINMAN_AUTOUPDATE_ALL_USERS_DATA
					                            + l_files4UpdateInAllUsersSettings.size()
#endif
					                           ) + ' ' + STRING(FILES) + "). ";
					message(l_message);
					
					const bool l_userAsk = BOOLSETTING(AUTOUPDATE_SHOWUPDATEREADY) || m_manualUpdate;
					if (m_guiDelegate && !ClientManager::isShutdown())
					{
						m_guiDelegate->NewVerisonEvent(Text::fromT(TSTRING(MENU_FLYLINK_NEW_VERSION)) + " " + l_autoUpdateObject->m_sVersion);
					}
					if (l_userAsk)
					{
						// [!]TODO Ask to setup
						// "FlylinkDC++ found new update %s ( %s ) and ready to download - %s. Continue this update?"
						l_message += STRING(AUTOUPDATE_DOWNLOAD_START3);
						//
						UpdateResult idResult = UPDATE_CANCEL;
						if (m_guiDelegate && !ClientManager::isShutdown())
						{
							// Download RTF file from Server
							string programRtfData;
#ifdef IRAINMAN_AUTOUPDATE_ALL_USERS_DATA
							//string basesRtfData;
#endif
							size_t l_dataRTFSize = Util::getDataFromInetSafe(true, programUpdateDescription, programRtfData);
							// l_programRtfData.resize(dataRTFSize); // TODO - зачем это?
#ifdef IRAINMAN_AUTOUPDATE_ALL_USERS_DATA
							//dataRTFSize = Util::getDataFromInetSafe(basesUpdateDescription, basesRtfData);
							//basesRtfData.resize(dataRTFSize);
#endif
							if (m_guiDelegate && !ClientManager::isShutdown())
							{
								idResult = UpdateResult(m_guiDelegate->ShowDialogUpdate(l_message, programRtfData, l_files4Description
#ifdef IRAINMAN_AUTOUPDATE_ALL_USERS_DATA
								                                                        //+ basesRtfData
#endif
								                                                       ));
							}
						}
						else
						{
							UINT iResult = UPDATE_IGNORE;
							if (!ClientManager::isShutdown())
							{
								iResult = ::MessageBox(m_mainFrameHWND, Text::toT(l_message).c_str(), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_YESNOCANCEL | MB_ICONQUESTION | MB_DEFBUTTON1);
							}
							switch (iResult)
							{
								case IDYES:
									idResult = UPDATE_NOW;
									break;
								case IDNO:
									idResult = UPDATE_IGNORE;
									break;
								case IDCANCEL:
								default:
									idResult = UPDATE_CANCEL;
							}
						}
						needToUpdate = idResult == UPDATE_NOW;
						if (!needToUpdate)
						{
							const string l_versionToIgnore = ' ' + l_autoUpdateObject->m_sVersion + " (" + l_autoUpdateObject->m_sUpdateDate + ')'
#ifdef IRAINMAN_AUTOUPDATE_ALL_USERS_DATA
							                                 // + "\nBases" + autoUpdateObjectAU->m_sVersion + " (" + autoUpdateObjectAU->m_sUpdateDate + ')'
#endif
							                                 ;
							if (idResult == UPDATE_IGNORE)
							{
								message(STRING(AUTOUPDATE_IGNORED) + l_versionToIgnore);
								SET_SETTING(AUTOUPDATE_IGNORE_VERSION, l_autoUpdateObject->m_sVersion);
							}
							else
							{
								message(STRING(AUTOUPDATE_TEMP_IGNORE) + l_versionToIgnore);
							}
						}
					}
					if (needToUpdate)
					{
						// Start file Uploading
						InetDownloadReporter::getInstance()->ReportResultWait(l_totalSize);
						string l_errorFileName;
						
						// Update Settings file at first
						if (preparingFiles(l_files4UpdateInSettings, Util::getConfigPath(), l_errorFileName))
#ifdef IRAINMAN_AUTOUPDATE_ALL_USERS_DATA
							if (preparingFiles(l_files4UpdateInAllUsersSettings, Util::getConfigPath(
#ifndef USE_SETTINGS_PATH_TO_UPDATA_DATA
							                       true
#endif
							                   ), l_errorFileName))
#endif
								if (preparingFiles(l_files4Update, g_tempFolder, l_errorFileName))
								{
									if (l_needsToStartUpdate && l_userAsk)
									{
										wstring wMessage = WSTRING(AUTOUPDATE_DOWNLOAD_FINISHED);
										if (BOOLSETTING(AUTOUPDATE_FORCE_RESTART))
										{
											wMessage += ' ';
											wMessage += WSTRING(AUTOUPDATE_DOWNLOAD_RESTART);
											l_needsToStartUpdate = ::MessageBox(m_mainFrameHWND, wMessage.c_str(), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1) == IDYES;
											if (!l_needsToStartUpdate)
											{
												message(STRING(AUTOUPDATE_CANCEL));
											}
										}
										else
										{
											l_needsToStartUpdate = true;
											wMessage += WSTRING(AUTOUPDATE_DOWNLOAD_UPDATEONEXIT);
											::MessageBox(m_mainFrameHWND, wMessage.c_str(), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_OK);
										}
									}
									
									if (l_needsToStartUpdate)
									{
										m_updateFolder = Text::toT(g_tempFolder);
										wstring flagName = m_updateFolder;
										AppendPathSeparator(flagName);
										flagName += UPDATE_FILE_NAME;
										try // [+] IRainman fix.
										{
											{
												File f(flagName, File::WRITE, File::CREATE | File::TRUNCATE); // [1] https://www.box.net/shared/75247d259e1ee4eab670
												f.close();
											}
											SET_SETTING(AUTOUPDATE_PATH_WITH_UPDATE, g_tempFolder);
											
											message(STRING(AUTOUPDATE_STARTED));
											if (BOOLSETTING(AUTOUPDATE_FORCE_RESTART))
											{
												g_exitOnUpdate = true;
												ClientManager::shutdown();
												PostMessage(m_mainFrameHWND, WM_CLOSE, 0, 0);
											}
										}
										catch (const FileException& /*e*/)
										{
											message(STRING(AUTOUPDATE_ERROR_UPDATE_FILE) + ' ' + Text::fromT(flagName) + g_dev_error);
										}
									}
#if defined(IRAINMAN_AUTOUPDATE_ALL_USERS_DATA) && defined (ALLOW_RELOAD_INTERNALBD_IN_RUNTIME)
									if (needsUpdateBases && !m_exitOnUpdate)
									{
										// TODO: use module name here (create list and use it).
										bool l_reloadGeoIP = false;
										for (size_t i = 0; i < l_files4UpdateInAllUsersSettings.size(); ++i)
										{
											const string& l_name = l_files4UpdateInAllUsersSettings[i].m_sName;
											// TODO - убрать имена зашитые в код!
											if (!l_reloadGeoIP && (l_name == "CustomLocations.bmp" || l_name == "CustomLocations.ini" || l_name == "GeoIPCountryWhois.csv" || l_name == "P2PGuard.ini" || l_name == "iblocklist-com.ini"))
											{
												l_reloadGeoIP = true;
											}
										}
										if (l_reloadGeoIP)
										{
											message(_T("Reload GeoIP in progress…"));
											Util::loadGeoIp(true);
											message(_T("Reload GeoIP done"));
										}
									}
#endif // IRAINMAN_AUTOUPDATE_ALL_USERS_DATA && ALLOW_RELOAD_INTERNALBD_IN_RUNTIME
								}
								else
								{
									if (l_userAsk)
									{
										const tstring l_message = TSTRING(AUTOUPDATE_DOWNLOAD_FAILED) + _T("\r\n") + TSTRING(FAILED_TO_LOAD) + _T("\r\n") + Text::toT(l_errorFileName);
										CFlyServerJSON::pushError(63, Text::fromT(l_message));
										if (!ClientManager::isShutdown())
										{
											::MessageBox(m_mainFrameHWND, l_message.c_str(), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_OK | MB_ICONERROR);
										}
									}
									else
									{
										message(STRING(AUTOUPDATE_ERROR_UPDATE_FILE) + ' ' + l_errorFileName);
									}
								}
					}
				}
				else
				{
					m_isUpdate = true;
					message(STRING(YOU_HAVE_THE_LATEST_VERSION));
					if (m_manualUpdate && !ClientManager::isShutdown())
					{
						::MessageBox(m_mainFrameHWND, CWSTRING(YOU_HAVE_THE_LATEST_VERSION), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_OK | MB_ICONINFORMATION);
					}
				}
			}
		}
		else
		{
			m_isUpdate = false;
			message(STRING(ADDRESS_FOR_UPDATE_ERROR));
		}
	}
	m_manualUpdate = false;
}

void AutoUpdate::fail(const string& p_error)
{
	dcdebug("AutoUpdate: New command when already failed: %s\n", p_error.c_str());
	const auto l_error = STRING(AUTOUPDATE) + ' ' + p_error;
	LogManager::message(l_error);
	CFlyServerJSON::pushError(62, l_error);
}

void AutoUpdate::message(const string& p_message)
{
	LogManager::message(STRING(AUTOUPDATE) + ' ' + p_message);
}

SettingsManager::IntSetting AutoUpdate::getSettingByTitle(const string& wTitle)
{
	if (wTitle == "exe")
		return SettingsManager::AUTOUPDATE_EXE;
	if (wTitle == "utilities")
		return SettingsManager::AUTOUPDATE_UTILITIES;
	if (wTitle == "lang")
		return SettingsManager::AUTOUPDATE_LANG;
	if (wTitle == "portalbrowser")
		return SettingsManager::AUTOUPDATE_PORTALBROWSER;
#ifdef IRAINMAN_INCLUDE_SMILE
	if (wTitle == "emopacks")
		return SettingsManager::AUTOUPDATE_EMOPACKS;
#endif
	if (wTitle == "webserver")
		return SettingsManager::AUTOUPDATE_WEBSERVER;
	if (wTitle == "sounds")
		return SettingsManager::AUTOUPDATE_SOUNDS;
	if (wTitle == "iconthemes")
		return SettingsManager::AUTOUPDATE_ICONTHEMES;
	if (wTitle == "colorthemes")
		return SettingsManager::AUTOUPDATE_COLORTHEMES;
	if (wTitle == "documentation")
		return SettingsManager::AUTOUPDATE_DOCUMENTATION;
#ifdef IRAINMAN_AUTOUPDATE_ALL_USERS_DATA
	if (wTitle == "geoip")
		return SettingsManager::AUTOUPDATE_GEOIP;
	if (wTitle == "customlocation")
		return SettingsManager::AUTOUPDATE_CUSTOMLOCATION;
#endif // IRAINMAN_AUTOUPDATE_ALL_USERS_DATA
#ifdef SSA_SHELL_INTEGRATION
	if (wTitle == "shellext")
		return SettingsManager::AUTOUPDATE_SHELL_EXT;
#endif
	/*
	if (wTitle == "chatbot")
	    return SettingsManager::AUTOUPDATE_UPDATE_CHATBOT;
	*/
	return SettingsManager::INT_LAST;
}

bool AutoUpdate::needUpdateFile(const AutoUpdateFile& p_file, const string& p_outputFolder)
{
	switch (p_file.m_sys)
	{
#ifdef IRAINMAN_AUTOUPDATE_ARCH_DIFFERENCE
#else // IRAINMAN_AUTOUPDATE_ARCH_DIFFERENCE
		case AutoUpdateFile::OsUnknown:
#if defined(_WIN64)
		case AutoUpdateFile::x86_legacy:
#elif defined(_WIN32)
		case AutoUpdateFile::x86_64:
#endif
			return false;
#endif // IRAINMAN_AUTOUPDATE_ARCH_DIFFERENCE
	}
	
	// Find file. If not found -> return true;
	//            if found. Check size, if not equal return true;
	//                                  if equal. Check TTH. If not equal return true;
	//                                                                    return false;
	string filePath = p_outputFolder;
	AppendPathSeparator(filePath);
	filePath += p_file.m_sPath;
	AppendPathSeparator(filePath);
	filePath += p_file.m_sName;
	
	int64_t outFileSize = 0;
	
	const FileFindIter fiter(filePath);
	if (fiter != FileFindIter::end)
	{
		if (fiter->getSize() == p_file.m_size)
		{
			// Calculate TTH (in string)
			const int c_size_buf = std::min(p_file.m_size, static_cast<int64_t>(1024 * 1024));
			unique_ptr<TigerTree>  tth;
			if (Util::getTTH_MD5(filePath, c_size_buf, &tth))
			{
				const string l_TTH_str = tth.get()->getRoot().toBase32();
				if (l_TTH_str == p_file.m_sTTH)
					return false;
			}
			else
			{
				// TODO
			}
		}
	}
	return true;
}

bool AutoUpdate::prepareFile(const AutoUpdateFile& file, const string& tempFolder)
{
	// string filenameX = "H:\\Projects\\flylinkdc500\\compiled\\FlylinkDC.exe";
	/*
	File fx(filenameX, File::READ, File::OPEN);
	int64_t fileXSize = fx.getSize();
	unique_ptr<byte[]> dataIn(new byte[fileXSize]);
	unique_ptr<byte[]> dataOut(new byte[fileXSize]);
	fx.read(dataIn.get(), (size_t&)fileXSize);
	fx.close();
	size_t inSize = fileXSize;
	size_t outSize = fileXSize;
	BZFilter bzip;
	if ( bzip(dataIn.get(), inSize, dataOut.get(), outSize) )
	{
	
	    unique_ptr<byte[]> dataOutTest(new byte[fileXSize]);
	    UnBZFilter unbzip;
	    unbzip(dataOut.get(), outSize, dataOutTest.get(), inSize);
	    File fy(filenameX+".bz2", File::WRITE, File::OPEN | File::CREATE);
	    fy.write( dataOut.get(), outSize);
	    fy.close();
	}
	*/
	
	if (file.m_sDownloadURL.empty())
	{
		return false;
	}
	
	std::vector<byte> l_binary_data;
	IDateReceiveReporter* reporter = InetDownloadReporter::getInstance();
	// 60 sec - http://code.google.com/p/flylinkdc/issues/detail?id=1403
	CFlyHTTPDownloader l_http_downloader;
	int64_t sizeRead = l_http_downloader.getBinaryDataFromInetSafe(file.m_sDownloadURL, l_binary_data, 60000, reporter); // TODO - передать размер буфера сразу
	if (sizeRead == file.m_packedSize)
	{
		/*
		File fy(filenameX+".bz2T", File::WRITE, File::OPEN | File::CREATE);
		fy.write( data.get(), sizeRead);
		fy.close();
		*/
		// Unpack if need
		size_t outSize = file.m_size + 1;
		unique_ptr<byte[]> out(new byte[file.m_size]);
		bool unPacked = false;
		if (file.m_packer == AutoUpdateFile::Zip)
		{
			UnZFilter unzip;
			unPacked  = (unzip(l_binary_data.data(), (size_t &)sizeRead, out.get(), outSize) && outSize == file.m_size);
		}
		else if (file.m_packer == AutoUpdateFile::BZip2)
		{
			UnBZFilter unbzip;
			try
			{
				unbzip(l_binary_data.data(), (size_t &)sizeRead, out.get(), outSize);
				unPacked = (outSize == file.m_size) ;
			}
			catch (const Exception&)
			{
				// Unpack error
				unPacked = false;
			}
		}
		else if (file.m_packer == AutoUpdateFile::Unpacked)
		{
			memcpy(out.get(), l_binary_data.data(), sizeRead);
			unPacked = true;
		}
		if (unPacked)
		{
			// Save out.get() to File
			string destinationPath = tempFolder;
			AppendPathSeparator(destinationPath);
			destinationPath += file.m_sPath;
			AppendPathSeparator(destinationPath);
			destinationPath += file.m_sName;
			wstring wdestinationPath = Text::toT(destinationPath);
			size_t pos = wdestinationPath.find(L'\\');
			while (pos != wstring::npos)
			{
				wstring folder = wdestinationPath.substr(0, pos);
				CreateDirectory(folder.c_str(), NULL);
				pos = wdestinationPath.find(L'\\', pos + 1);
			}
			try
			{
				File f(destinationPath, File::WRITE, File::TRUNCATE | File::CREATE);
				f.write(out.get(), outSize);
				return true;
			}
			catch (const FileException& e)
			{
				const string l_ErrorString = e.getError() + ' ' + destinationPath + g_dev_error;
				const string l_error = STRING(ERROR_STRING) + '[' + l_ErrorString + ']';
				fail(l_error);
				CFlyServerJSON::pushError(63, l_error);
				::MessageBox(nullptr, Text::toT(l_ErrorString).c_str(), getFlylinkDCAppCaptionWithVersionT().c_str() , MB_ICONSTOP);
				return false;
			}
		}
	}
	else
	{
		fail("Util::getDataFromInet error: sizeRead = " +
		     Util::toString(sizeRead) + " file.m_packedSize = " + Util::toString(file.m_packedSize) +
		     " [ " + file.m_sDownloadURL + " ]" + " Error: " + l_http_downloader.getErroMessage());
	}
	
	return false;
}

string AutoUpdate::getTempFolderForUpdate(const string &version)
{
	string tempFolder = Util::getTempPath();
	AppendPathSeparator(tempFolder);
	tempFolder += "FlylinkDC_Update";
	const wstring wtempFolder = Text::toT(tempFolder);
	CreateDirectory(wtempFolder.c_str(), NULL);
	
	AppendPathSeparator(tempFolder);
	tempFolder += version;
	
	string tempFolderPath;
	bool bFolderfound  = true;
	int i = 0;
	do
	{
		i++;
		tempFolderPath = tempFolder + '_' + Util::toString(i);
		const FileFindIter fiter(tempFolderPath);
		bFolderfound = (fiter != FileFindIter::end);
		if (!bFolderfound)
		{
			const wstring wtempFolderPath = Text::toT(tempFolderPath);
			if (CreateDirectory(wtempFolderPath.c_str(), NULL) == NULL)
				bFolderfound = true;
		}
		if (i == 999)
		{
			tempFolderPath.clear();
			break;
		}
	}
	while (bFolderfound);
	
	return tempFolderPath;
}

bool AutoUpdate::verifyUpdate(byte* sig, size_t sig_len, const string& data, size_t dataSize)
{
	if (sig_len == 0 || dataSize == 0)
		return false;
	// Get Public Key
	RSA* r = nullptr;
	int rc = -1;
	do
	{
		// Make SHA hash
		SHA_CTX sha_ctx = { 0 };
		byte digest[SHA_DIGEST_LENGTH] = {0};
		rc = SHA1_Init(&sha_ctx);
		if (rc != 1)
			break;
		rc = SHA1_Update(&sha_ctx, data.c_str(), data.length());
		if (rc != 1)
			break;
		rc = SHA1_Final(digest, &sha_ctx);
		if (rc != 1)
			break;
		// Extract Key
		const byte* p = sFlylinkDCPublicKey;
		r = d2i_RSAPublicKey(NULL, &p, sizeof(sFlylinkDCPublicKey));
		rc = RSA_verify(NID_sha1, digest, sizeof(digest), sig, sig_len, r); //-V107
		if (rc != 1)
			break;
	}
	while (false);
	if (r)
	{
		RSA_free(r);
	}
	if (rc == 1)
		return true;
	else
		return false;
}

const string AutoUpdate::UPDATE_FILE_DOWNLOAD()
{
	return BOOLSETTING(AUTOUPDATE_TO_BETA) ? UPDATE_FILE_DOWNLOAD_B : UPDATE_FILE_DOWNLOAD_R;
}

const string AutoUpdate::UPDATE_SIGN_FILE_DOWNLOAD()
{
	return BOOLSETTING(AUTOUPDATE_TO_BETA) ? UPDATE_SIGN_FILE_DOWNLOAD_B : UPDATE_SIGN_FILE_DOWNLOAD_R;
}

const string AutoUpdate::UPDATE_DESCRIPTION()
{
	return BOOLSETTING(AUTOUPDATE_TO_BETA) ? UPDATE_DESCRIPTION_B : UPDATE_DESCRIPTION_R;
}

string AutoUpdate::getAUTOUPDATE_SERVER_URL()
{
	// SSA - get custom URL if need
	const string l_customUpdateServerUrl = SETTING(AUTOUPDATE_SERVER_URL);
	if (BOOLSETTING(AUTOUPDATE_USE_CUSTOM_URL) && !l_customUpdateServerUrl.empty())
	{
		return l_customUpdateServerUrl;
	}
	return BOOLSETTING(AUTOUPDATE_TO_BETA) ? UPDATE_BETA_URL : UPDATE_RELEASE_URL;
}

AutoUpdateFile AutoUpdate::parseNode(const XMLParser::XMLNode& Node)
{
	AutoUpdateFile file;
	file.m_sName  = Node.getAttributeOrDefault("Name");
	file.m_sPath  = Node.getAttributeOrDefault("Path");
	const string wSys   = Node.getAttributeOrDefault("Sys");
	const string wPack  = Node.getAttributeOrDefault("Pack");
	file.m_size = Util::toInt64(Node.getAttributeOrDefault("Size"));
	file.m_sTTH = Node.getAttributeOrDefault("TTH");
	const string wPackedSize  = Node.getAttributeOrDefault("PackedSize", "0");
	file.m_sDownloadURL   = Node.getAttributeOrDefault("Url");
	
	if (wSys.empty())
		file.m_sys = AutoUpdateFile::xAll;
	else if (wSys == "x86")
		file.m_sys = AutoUpdateFile::x86_legacy;
	else if (wSys == "x64")
		file.m_sys = AutoUpdateFile::x86_64;
#ifdef IRAINMAN_AUTOUPDATE_ARCH_DIFFERENCE
#endif
	else
		file.m_sys = AutoUpdateFile::OsUnknown;
		
	if (wPack.empty())
		file.m_packer = AutoUpdateFile::Unpacked;
	else if (wPack == "zip")
		file.m_packer = AutoUpdateFile::Zip;
	else if (wPack == "bz2")
		file.m_packer = AutoUpdateFile::BZip2;
	else
		file.m_packer = AutoUpdateFile::PackUnknown;
		
	if (!Util::isHttpLink(file.m_sDownloadURL))
	{
		string serverURL = getAUTOUPDATE_SERVER_URL();
		if (!serverURL.empty())
		{
			AppendUriSeparator(serverURL);
			file.m_sDownloadURL = serverURL + file.m_sDownloadURL;
		}
	}
	
	if (file.m_packer == AutoUpdateFile::Unpacked)
		file.m_packedSize = file.m_size;
	else
		file.m_packedSize = Util::toInt64(wPackedSize);
		
	return file;
}

void AutoUpdate::runFlyUpdate()
{
	if (!m_updateFolder.empty())
	{
		CompatibilityManager::restoreProcessPriority(); // [+] IRainman
		
		const wstring flyUpdatepath = Text::toT(Util::getConfigPath());
		const wstring flylinkDC = Util::getModuleFileName();
		
		wstring parameters = L'\"' + flylinkDC + L"\" \"";
		parameters += m_updateFolder;
		parameters += g_exitOnUpdate ? L"\" 1" : L"\" 0";
		
		const uint64_t processID = ::GetCurrentProcessId(); // TODO DWORD
		parameters += L' ' + Util::toStringW(processID); // TODO DWORD - please update function Util::toStringW
		parameters += L" 1";
		
		SHELLEXECUTEINFO shex = {0};
		
		shex.cbSize         = sizeof(SHELLEXECUTEINFO);
		//shex.fMask          = 0;
		//shex.hwnd           = NULL; //(HWND)*this;
		shex.lpVerb         = L"open";
		shex.lpFile         = L"FlyUpdate.exe";// flyUpdate.c_str();
		shex.lpParameters   = parameters.c_str();
		shex.lpDirectory    = flyUpdatepath.c_str(); //updateApplication.c_str();
		shex.nShow          = SW_NORMAL;
		
		if (::ShellExecuteEx(&shex))
		{
			// Remove Flag 'cause of starting flyUpdate
			wstring flagName = m_updateFolder;
			AppendPathSeparator(flagName);
			flagName += UPDATE_FILE_NAME;
			File::deleteFileT(flagName);
		}
		else
		{
			const tstring l_error = TSTRING(AUTOUPDATE_ERROR_START_FLYUPDATE_FAILED) + Text::toT(Util::translateError());
			CFlyServerJSON::pushError(63, Text::fromT(l_error));
			MessageBox(nullptr, l_error.c_str(), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_OK | MB_ICONERROR);
		}
	}
}

bool AutoUpdate::startupUpdate()
{
	const string& l_settingsUpdateFolder = SETTING(AUTOUPDATE_PATH_WITH_UPDATE);
	if (!l_settingsUpdateFolder.empty())
	{
		m_updateFolder = Text::toT(l_settingsUpdateFolder);
		FileUpdateSearch fuSearch(m_updateFolder);
		fuSearch.Scan();
		if (fuSearch.GetFilesCount() > 0 || fuSearch.GetFoldersCount() > 0)     // SSA - Can be only folders for example...
		{
			// Find Flag or delete
			tstring flagName = m_updateFolder;
			AppendPathSeparator(flagName);
			flagName += UPDATE_FILE_NAME;
			if (File::isExist(flagName))
			{
				// Run Update and Exit
				if (MessageBox(m_mainFrameHWND, CTSTRING(AUTOUPDATE_DOWNLOADED_REMOVE_OR_INSTALL), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1) == IDYES)
				{
					g_exitOnUpdate = true;
					ClientManager::shutdown();
					return true;
				}
			}
			fuSearch.DeleteAll();
		}
		::RemoveDirectory(m_updateFolder.c_str());
		SET_SETTING(AUTOUPDATE_PATH_WITH_UPDATE, Util::emptyString);
		m_updateFolder.clear();
	}
	return false;
}
