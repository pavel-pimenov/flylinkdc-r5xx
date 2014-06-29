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

#include "stdinc.h"
#include <fstream>
#include "LogManager.h"
#include "SimpleXML.h"
#include "AdcHub.h"
#include "CID.h"
#include "StringTokenizer.h"
#ifdef STRONG_USE_DHT
#include "../dht/dht.h"
#endif
//[+]IRainman SpeedLimiter
#include "UploadManager.h"
#include "ThrottleManager.h"
//[~]IRainman SpeedLimiter
#include "ShareManager.h" // [+] NightOrion
#include <boost/algorithm/string.hpp>
#include "../FlyFeatures/AutoUpdate.h"
#include "ConnectivityManager.h"

StringList SettingsManager::g_connectionSpeeds;
boost::logic::tribool SettingsManager::g_TestUDPSearchLevel = boost::logic::indeterminate;
boost::logic::tribool SettingsManager::g_TestUDPDHTLevel = boost::logic::indeterminate;
boost::logic::tribool SettingsManager::g_TestTCPLevel = boost::logic::indeterminate;
boost::logic::tribool SettingsManager::g_TestTSLLevel = boost::logic::indeterminate;

string SettingsManager::g_UDPTestExternalIP;


// [!] IRainman opt: use this data as static.
string SettingsManager::strSettings[STR_LAST - STR_FIRST];
int    SettingsManager::intSettings[INT_LAST - INT_FIRST];

string SettingsManager::strDefaults[STR_LAST - STR_FIRST];
int    SettingsManager::intDefaults[INT_LAST - INT_FIRST];

bool SettingsManager::isSet[SETTINGS_LAST];

// Search types
SettingsManager::SearchTypes SettingsManager::g_searchTypes; // name, extlist
// [!] IRainman opt: use this data as static.

const string SettingsManager::settingTags[] =
{
	// Strings //
	
	"Nick", "UploadSpeed", "Description", "DownloadDirectory", "EMail", "ExternalIp",
	
	"TextFont", "MainFrameOrder", "MainFrameWidths", "HubFrameOrder", "HubFrameWidths",
	"LanguageFile", "SearchFrameOrder", "SearchFrameWidths", "FavoritesFrameOrder", "FavoritesFrameWidths", "FavoritesFrameVisible",
	"HublistServers", "QueueFrameOrder", "QueueFrameWidths", "PublicHubsFrameOrder", "PublicHubsFrameWidths", "PublicHubsFrameVisible",
	"UsersFrameOrder", "UsersFrameWidths", "UsersFrameVisible", "HttpProxy", "LogDir", "NotepadText", "LogFormatPostDownload",
	
	"LogFormatPostUpload", "LogFormatMainChat", "LogFormatPrivateChat", "FinishedOrder", "FinishedWidths",
	"TempDownloadDirectory", "BindAddress", "SocksServer", "SocksUser", "SocksPassword", "ConfigVersion",
	
	"DefaultAwayMessage", "TimeStampsFormat", "ADLSearchFrameOrder", "ADLSearchFrameWidths", "ADLSearchFrameVisible",
	"FinishedULWidths", "FinishedULOrder", "CID", "SpyFrameWidths", "SpyFrameOrder", "SpyFrameVisible",
	
	"SoundBeepFile", "SoundBeginFile", "SoundFinishedFile", "SoundSourceFile", "SoundUploadFile", "SoundFakerFile", "SoundChatNameFile", "SoundTTH", "SoundHubConnected", "SoundHubDisconnected", "SoundFavUserOnline", "SoundFavUserOffline", "SoundTypingNotify", "SoundSearchSpy",
	"KickMsgRecent01", "KickMsgRecent02", "KickMsgRecent03", "KickMsgRecent04", "KickMsgRecent05",
	"KickMsgRecent06", "KickMsgRecent07", "KickMsgRecent08", "KickMsgRecent09", "KickMsgRecent10",
	"KickMsgRecent11", "KickMsgRecent12", "KickMsgRecent13", "KickMsgRecent14", "KickMsgRecent15",
	"KickMsgRecent16", "KickMsgRecent17", "KickMsgRecent18", "KickMsgRecent19", "KickMsgRecent20",
	"Toolbar",
	"ToolbarImage", "ToolbarHot", "UserListImage",
	"UploadQueueFrameOrder", "UploadQueueFrameWidths",
	
	"ProfilesURL", "WinampFormat",
	
	"WebServerPowerUser", "WebServerPowerPass", "webServerBindAddress", // [+] IRainman
	"WebServerLogFormat", "LogFormatCustomLocation", "LogFormatTraceSQLite", "LogFormatDdosTrace", "LogFormatDHTTrace", "LogFormatPSRTrace", "WebServerUser", "WebServerPass", "LogFileMainChat",
	"LogFilePrivateChat", "LogFileStatus", "LogFileUpload", "LogFileDownload", "LogFileSystem", "LogFormatSystem",
	"LogFormatStatus", "LogFileWebServer", "LogFileCustomLocation", "LogTraceSQLite", "LogFileDdosTrace", "LogFileDHTTrace", "LogFilePSRTrace",
	"DirectoryListingFrameOrder", "DirectoryListingFrameWidths",
	"MainFrameVisible", "SearchFrameVisible", "QueueFrameVisible", "HubFrameVisible", "UploadQueueFrameVisible",
	"EmoticonsFileFlylinkDC",
	"TLSPrivateKeyFile", "TLSCertificateFile", "TLSTrustedCertificatesPath",
	"FinishedVisible", "FinishedULVisible", "BetaUser", "BetaPass", "AuthPass", "SkiplistShare", "DirectoryListingFrameVisible",
	
	"RecentFrameOrder", "RecentFrameWidths", "RecentFrameVisible", "HighPrioFiles", "LowPrioFiles", "SecondaryAwayMsg", "ProtectedPrefixes", "WMPFormat",
	"iTunesFormat", "MPCFormat", "RawOneText", "RawTwoText", "RawThreeText", "RawFourText", "RawFiveText", "PopupFont", "PopupTitleFont", "PopupFile",
	
	"BanMessage", "SlotAsk", // !SMT!-S
	"UrlGetIp", //[+]PPA
	"Password", "PasswordHint", "PasswordOkHint",// !SMT!-PSW
	"WebMagnet",//[+]necros
	"RatioTemplate",//[+] WhiteD. Custom ratio message
	"UrlIPTrust", //[+]PPA
	"ToolBarSettings",
	"WinampToolBar", //[+] Drakon!
#ifdef RIP_USE_LOG_PROTOCOL
	"LogFileProtocol",
	"LogFormatProtocol",
#endif
	"CustomMenuURL", // [+] SSA
	"RssNewsOrder",   // [+] SSA
	"RssNewsWidth",   // [+] SSA
	"RssNewsVisible", // [+] SSA
#ifdef STRONG_USE_DHT
	"DHTKey",
#endif
	"Mapper",
	"PortalBrowserUpdateURL",// [+] PPA
	"ISPResourceRootURL",// [+] PPA
	"BTMagnetOpenCMD",//[+]IRainman
	//"UpdateChanel", // [+] IRainman TODO update chanels
	"ThemeDLLName", // [+] SSA - ThemeDLLName, if emty - use standart resource
	"ThemeManagerSoundsThemeName", // [+] SCALOlaz: Sounds Theme
	"UpdateServerURL", // [+] SSA
	"UpdateIgnoreVersion", // [+] SSA
	"UpdatePathWithUpdate", // [+] SSA
	"JetAudioFormat", // [+] SSA
	"QcdQmpFormat",
	"DclstFolder", // [+] SSA
	"PreviewClientPath", // [+] SSA
	"SavedSearchSize",
	"FlyLocatorCountry",
	"FlyLocatorCity",
	"FlyLocatorISP",
//	"MainDomain",
	"SENTRY",
	
	// Ints //
	
	"IncomingConnections", "ForcePassiveIncomingConnections", "InPort", "Slots", "AutoFollow", "ClearSearch",
	"BackgroundColor", "TextColor", "ShareHidden",
	"ShareVirtual", "ShareSystem", //[+] IRainman
	"FilterMessages", "MinimizeToTray",
	"AutoSearch", "TimeStamps", "ConfirmExit",
	"SuppressPms", //[+] IRainman
	"PopupHubPms", "PopupBotPms", "IgnoreHubPms", "IgnoreBotPms",
	"BufferSizeForDownloads", "DownloadSlots", "MaxDownloadSpeed", "LogMainChat", "LogPrivateChat",
	"LogDownloads", "LogUploads",
	"LogifSuppressPms", // [+]IRainman
	"StatusInChat", "ShowJoins", "PrivateMessageBeep", "PrivateMessageBeepOpen",
	"UseSystemIcons", "PopupPMs", "MinUploadSpeed", "UrlHandler", "MainWindowState",
	"MainWindowSizeX", "MainWindowSizeY", "MainWindowPosX", "MainWindowPosY", "AutoAway",
	"SocksPort", "SocksResolve", "KeepLists", "AutoKick", "QueueFrameShowTree", "QueueFrameSplit",
	"CompressTransfers", "ShowProgressBars", "MaxTabRows",
	"MaxCompression", "AntiFragMethod", "AntiFragMax", "MDIMaxmimized",
	// [-] "NoAwayMsgToBots", [-] IRainman fix.
	"SkipZeroByte", "AdlsBreakOnFirst",
	"HubUserCommands",
	"SendBloom",
	"AutoSearchAutoMatch", "DownloadBarColor", "UploadBarColor", "LogSystem",
	"LogCustomLocation", // [+] IRainman
	"LogSQLiteTrace", "LogDDOSTrace", "LogDHTTrace", "LogPSRTrace",
	"LogFilelistTransfers", "ShowStatusbar", "ShowToolbar", "ShowTransferview", "ShowTransferViewToolbar",
	"SearchPassiveAlways", "SetMinislotSize", "ShutdownInterval",
	//"CzertHiddenSettingA", "CzertHiddenSettingB",// [-] IRainman SpeedLimiter
	"ExtraSlots",
	"TextGeneralBackColor", "TextGeneralForeColor", "TextGeneralBold", "TextGeneralItalic",
	"TextMyOwnBackColor", "TextMyOwnForeColor", "TextMyOwnBold", "TextMyOwnItalic",
	"TextPrivateBackColor", "TextPrivateForeColor", "TextPrivateBold", "TextPrivateItalic",
	"TextSystemBackColor", "TextSystemForeColor", "TextSystemBold", "TextSystemItalic",
	"TextServerBackColor", "TextServerForeColor", "TextServerBold", "TextServerItalic",
	"TextTimestampBackColor", "TextTimestampForeColor", "TextTimestampBold", "TextTimestampItalic",
	"TextMyNickBackColor", "TextMyNickForeColor", "TextMyNickBold", "TextMyNickItalic",
	"TextFavBackColor", "TextFavForeColor", "TextFavBold", "TextFavItalic",
	"TextOPBackColor", "TextOPForeColor", "TextOPBold", "TextOPItalic",
	"TextURLBackColor", "TextURLForeColor", "TextURLBold", "TextURLItalic",
	"TextEnemyBackColor", "TextEnemyForeColor", "TextEnemyBold", "TextEnemyItalic",
	"BoldAuthorsMess", "WindowsStyleURL", "UploadLimitNormal", "ThrottleEnable", "HubSlots", "DownloadLimitNormal",
	"UploadLimitTime", "DownloadLimitTime", "TimeThrottle", "TimeLimitStart", "TimeLimitEnd",
	"RemoveForbidden", "ProgressTextDown", "ProgressTextUp", "ShowInfoTips", "ExtraDownloadSlots",
	"MinimizeOnStratup", "ConfirmDelete", "DefaultSearchFreeSlots",
	"ExtensionDownTo", "ErrorColor", "TransferSplitSize", "IDownSpeed", "HDownSpeed", "DownTime",
	"ProgressOverrideColors", "Progress3DDepth", "ProgressOverrideColors2",
	"MenubarTwoColors", "MenubarLeftColor", "MenubarRightColor", "MenubarBumped",
	"DisconnectingEnable", "MinFileSize", "UploadQueueFrameShowTree", "UploadQueueFrameSplit",
	"SegmentsManual", "NumberOfSegments", "PercentFakeShareTolerated",
	"SendUnknownCommands", "Disconnect",
	"AutoUpdateIP", "AutoUpdateIPInterval",
	"MaxHashSpeed",
	"SaveTthInNtfsFilestream", "SetMinLenghtTthInNtfsFilestream", // [+] NightOrion
	"UseAutoPriorityByDefault", "UseOldSharingUI",
	"FavShowJoins", "LogStatusMessages", "PMLogLines", "SearchAlternateColour", "SoundsDisabled",
	"PopupsDisabled", // [+] InfinitySky.
	"PopupsTabsEnabled",    // [+] SCALOlaz
	"PopupsMessagepanelEnabled",
	"Use12HourFormat", // [+] InfinitySky.
	"MinimizeOnClose", // [+] InfinitySky.
	"ShowCustomMiniIconOnTaskbar", // [+] InfinitySky.
	"ShowCurrentSpeedInTitle", // [+] InfinitySky.
	"ShowFullHubInfoOnTab", // [+] NightOrion
	"ReportFoundAlternates", "CheckNewUsers", "GarbageIn", "GarbageOut",
	"SearchTime", "DontBeginSegment", "DontBeginSegmentSpeed", "PopunderPm", "PopunderFilelist",
	"DropMultiSourceOnly", "DisplayCheatsInMainChat", "MagnetAsk", "MagnetAction", "MagnetRegister",
	"DisconnectRaw", "TimeoutRaw", "FakeShareRaw", "ListLenMismatch", "FileListTooSmall", "FileListUnavailable",
	"Away", "UseCTRLForLineHistory",
	
	"PopupHubConnected", "PopupHubDisconnected", "PopupFavoriteConnected", "PopupFavoriteDisconnected", "PopupCheatingUser",
	"PopupChatLine", // [+] ProKK
	"PopupDownloadStart",
	
	"PopupDownloadFailed", "PopupDownloadFinished", "PopupUploadFinished", "PopupPm", "PopupNewPM", "PopupSearchSpy",
	"PopupType", "WebServer", "WebServerPort",
	"WebServerSearchSize", "WebServerSearchPageSize", "WebServerAllowChangeDownloadDIR", "WebServerAllowUPnP",// [+] IRainman
	"WebServerLog", "ShutdownAction", "MinimumSearchInterval",
	"PopupAway", "PopupMinimized", "ShowShareCheckedUsers", "MaxAutoMatchSource",
	"ReservedSlotColor", "IgnoredColor", "FavoriteColor",
	"NormalColour", "FireballColor", "ServerColor", "PasiveColor", "OpColor",
	"FileListAndClientCheckedColour", "BadClientColour", "BadFilelistColour", "DontDLAlreadyShared", "RealTimeQueueUpdate",
	"ConfirmOpenInetHubs", // [+] InfinitySky.
	"ConfirmHubRemoval", "ConfirmHubgroupRemoval", "ConfirmUserRemoval", "SuppressMainChat", "ProgressBackColor", "ProgressCompressColor", "ProgressSegmentColor",
	"OpenNewWindow", "FileSlots",  "UDPPort", "EnableMultiChunk",
	"UserListDoubleClick", "TransferListDoubleClick", "ChatDoubleClick", "AdcDebug", "NmdcDebug",
	"ToggleActiveWindow", "ProgressbaroDCStyle", "SearchHistory",
	//"BadSoftDetections", "DetectBadSoft", [-]
	//"AdvancedResume", // [-] merge
	"AcceptedDisconnects", "AcceptedTimeouts",
	"OpenPublic", "OpenFavoriteHubs", "OpenFavoriteUsers", "OpenQueue", "OpenFinishedDownloads",
	"OpenFinishedUploads", "OpenSearchSpy", "OpenNetworkStatistics", "OpenNotepad", "OutgoingConnections",
	"NoIPOverride", "ForgetSearchRequest", "SaveSearchSettings", "SavedSearchType", "SavedSearchSizeMode", "SavedSearchMode", "BoldFinishedDownloads",
	"BoldFinishedUploads", "BoldQueue",
	"BoldHub", "BoldPm", "BoldSearch", "BoldNewrss", "TabsPos",
	"HubPosition", // [+] InfinitySky.
	"SocketInBuffer2", "SocketOutBuffer2",
	"ColorRunning", "ColorDownloaded", "ColorVerified", "ColorAvoiding", "AutoRefreshTime", "OpenWaitingUsers",
	"BoldWaitingUsers", "NotboldFontOnActivityTab", "AutoSearchLimit", "AutoKickNoFavs", "PromptPassword", "SpyFrameIgnoreTthSearches",
	"TLSPort", "UseTLS", "MaxCommandLength", "AllowUntrustedHubs", "AllowUntrustedClients",
	"FastHash", "DownConnPerSec",
	"HighestPrioSize", "HighPrioSize", "NormalPrioSize", "LowPrioSize", "LowestPrio",
#ifdef IRAINMAN_ENABLE_SLOTS_AND_LIMIT_IN_DESCRIPTION
	"ShowDescription", "ShowDescriptionSlots", "ShowDescriptionLimit",
#endif
	"ProtectTray", "ProtectStart", "ProtectClose",
	"StripTopic", "TbImageSize", "TbImageSizeHot", "OpenCdmDebug", "ShowWinampControl", "HubThreshold", "PGOn", "GuardUp", "GuardDown", "GuardSearch", "PGLog",
	"PreviewPm", "FilterEnter", "PopupTime", "PopupW", "PopupH", "PopupTransp", "AwayThrottle", "AwayStart", "AwayEnd", "OdcStyleBumped", "TopSpeed", "StealthyStyle",
	"StealthyStyleIco", "StealthyStyleIcoSpeedIgnore", "PSRDelay",
	"IpInChat", "CountryInChat", "TopUpSpeed", "Broadcast", "RememberSettingsPage", "Page", "RememberSettingsWindowPos", "SettingsWindowPosX", "SettingsWindowPosY",
	"SettingsWindowSizeX", "SettingsWindowSizeYY", "SettingsWindowTransp", "SettingsWindowColorize", "SettingsWindowWikihelp", "ChatBufferSize", "EnableHubmodePic",
	"EnableCountryflag", "PgLastUp",
	"DiredtorListingFrameSplit",
#ifdef IRAINMAN_INCLUDE_TEXT_FORMATTING
	"FormatBIU",
#endif
	"MediaPlayer", "ProtFavs", "MaxMsgLength", "PopupBackColor", "PopupTextColor", "PopupTitleTextColor", "PopupImage", "PopupColors", "SortFavUsersFirst",
	"ShowShellMenu", "OpenLogsInternal",
	// "NoEmotesLinks", [-] IRainman
	
	"NsLookupMode", "NsLookupDelay", // !SMT!-IP
	"EnableAutoBan",// [+] IRainman
#ifdef IRAINMAN_ENABLE_OP_VIP_MODE
	"AutoBanProtectOP",
#endif
	"BanSlots", "BanSlotsH", /*old "BanShareMax",*/ "BanShare", "BanLimit", "BanmsgPeriod", "BanStealth", "BanForcePM", /* old "BanSkipOps",*/ "ExtraSlotToDl", // !SMT!-S
	"BanColor", "DupeColor", "MultilineChatInput", "ExternalPreview", "SendSlotgrantMsg", "FavUserDblClick",// !SMT!-UI
	"ProtectPrivate", "ProtectPrivateRnd", "ProtectPrivateSay", // !SMT!-PSW
	"UploadTransfersColors", // [+] Drakon
	"StartupBackup", // [+] Drakon
	
	"GlobalHubFrameConf",
	"IgnoreUseRegExpOrWc", "StealthyIndicateSpeeds",
	"ColourDupe", "NoTigerTreeHashCheat",
	"DeleteChecked", "Topmost", "LockToolbars",
	//"AutoCompleteSearch",//[-]IRainman always is true!
	"KeepDLHistory", "KeepULHistory", "ShowQSearch",
	"SearchDetectHash", "FullFileListNfo", "UseTabsCloseButton", "AltTypeTabsCloseButton",
	"ViewGridcontrols", // [+] ZagZag
	"DupeEx1Color", "DupeEx2Color", "IgnoreMe",// [+] NSL
	"EnableLastIP", //[+]PPA
	"EnableFlyServer", //[+]PPA
	"EnableHitFileList", //[+]PPA
	"EnableRatioUserList", //[+]PPA
	"NonHubsFront", "BlendOffline", "MaxResizeLines",
	"UseCustomListBackground",
	"EnableIpGuard", "DefaultPolicy",
#ifdef FLYLINKDC_LOG_IN_SQLITE_BASE
	"FlyTextLog", "UseSQLiteLog",
#endif // FLYLINKDC_LOG_IN_SQLITE_BASE
#ifdef RIP_USE_CONNECTION_AUTODETECT
	"IncomingAutodetectFlag",
#endif
	"LogProtocol",
	"TabSelectedColor",
	"TabOfflineColor",
	"TabActivityColor",
	"TabSelectedTextColor",
	"TabOfflineTextColor",
	"TabActivityTextColor",
	"HubInFavoriteBkColor",
	"HubInFavoriteConnectBkColor",
	"ChatAnimSmiles",
	"SmileSelectWndAnimSmiles",
	"UseCustomMenu", // [+] SSA
	"RssAutoRefreshTime", // [+] SSA
	"OpenRSSNews", // [+] SSA
	"PopupNewRSS", // [+] SSA
	"RssColumnsSort",   // [+] SCALOlaz
	"RssColumnsSortAsc",    // [+] SCALOlaz
	"SearchSpyColumnsSort", // [+] SCALOlaz
	"SearchSpyColumnsSortAsc",  // [+] SCALOlaz
	"SearchColumnsSort",    // [+] SCALOlaz
	"SearchColumnsSortAsc", // [+] SCALOlaz
	"QueueColumnsSort", // [+] SCALOlaz
	"QueueColumnsSortAsc",  // [+] SCALOlaz
	"FinishedDlColumnsSort",    // [+] SCALOlaz
	"FinishedDlColumnsSortAsc", // [+] SCALOlaz
	"FinishedUlColumnsSort",    // [+] SCALOlaz
	"FinishedUlColumnsSortAsc", // [+] SCALOlaz
	"UploadQueueColumnsSort",   // [+] SCALOlaz
	"UploadQueueColumnsSortAsc",    // [+] SCALOlaz
	"UsersColumnsSort", // [+] SCALOlaz
	"UsersColumnsSortAsc",  // [+] SCALOlaz
	"HubsPublicColumnsSort",    // [+] SCALOlaz
	"HubsPublicColumnsSortAsc", // [+] SCALOlaz
	"HubsFavoritesColumnsSort", // [+] SCALOlaz
	"HubsFavoritesColumnsSortAsc",  // [+] SCALOlaz
	"HubsRecentsColumnsSort",   // [+] SCALOlaz
	"HubsRecentsColumnsSortAsc",    // [+] SCALOlaz
	"DirlistColumnsSort",   // [+] SCALOlaz
	"DirlistColumnsSortAsc",    // [+] SCALOlaz
	"HubframeColumnsSort",  // [+] SCALOlaz
	"HubframeColumnsSortAsc",   // [+] SCALOlaz
	"TransfersColumnsSort", // [+] SCALOlaz
	"TransfersColumnsSortAsc",  // [+] SCALOlaz
	"Queued",
	"OverlapChunks",
	"ExtraPartialSlots",
	"AutoSlot",
#ifdef STRONG_USE_DHT
	"UseDHT",
	"UseDHTNotAnswer",
	"DHTPort",
	"UpdateIPDHT",
#endif
	"KeepFinishedFilesOption",
	"AllowNATTraversal", "UseExplorerTheme", "AutoDetectIncomingConnection",
	//"BetaInfo", //[+] NightOrion [-]
#ifdef RIP_USE_PORTAL_BROWSER
	"OpenPortalBrowser", //[+] BRAIN_RIPPER
#endif
	"MinMultiChunksSize", // [+] IRainman
	"MediainfoMinSize", //[+]PPA
#ifndef IRAINMAN_TEMPORARY_DISABLE_XXX_ICON
	"Gender", // [+] PPA
#endif
	"ShowSeekersInSpyFrame", // [+] IRainman
	"FormatBotMessage",// [+] IRainman
#ifdef IRAINMAN_USE_BB_CODES
	"FormatBBCode", // [+] IRainman
#endif
	"ReduceProcessPriorityIfMinimized", // [+] IRainman
	"MultilineChatInput_By_CtrlEnter", // [+] SSA
	"ShowSendMessageButton", // [+] SSA
	"ShowBBCodePanel", // [+] SSA
	"ShowEmothionButton", // [+] SSA
	"ShowMultiChatButton", // [+] IRainman
	"ChatRefferingToNick", // [+] SCALOlaz
	"UseAutoMultilineChat", // [+] SSA
	"UseMagnetsInPlayerSpam", // [+] SSA
	"UseBitrateFixForSpam", // [+] SSA
	"OnDownloadSetting", // [+] SSA
	// [-] "FileListFromZeroShareUsers", //[+] SSA [-] IRainman fix.
	"AutoUpdateEnable",
	"AutoUpdateRunOnStartup",
	"AutoUpdateRunAtTime",
	"AutoUpdateShowReady",
	"AutoUpdateTime",
	"AutoUpdateUpdateUnknown",
	"AutoUpdateExe",
	"AutoUpdateUtilities",
	"AutoUpdateLang",
	"AutoUpdatePortalBrowser",
	"AutoUpdateEmoPacks",
	"AutoUpdateWebServer",
	"AutoUpdateSounds",
	"AutoUpdateIconThemes",
	"AutoUpdateColorThemes",
	"AutoUpdateDocumentation",
	"AutoUpdateChatBot",
	"AutoUpdateForceRestart",
	"ExtraSlotByIP",
	"DCLSRegister", // [+] IRainman dcls support
	"AutoUpdateToBeta", // [+] SSA - update to beta version
	"AutoUpdateUseCustomURL", // [+] SSA
	"DclstCreateInSameFolder", // [+] SSA
	"DCLSTAsk", // [+] SSA
	"DCLSTAction", // [+] SSA
	"DCLSTIncludeSelf", // [+] SSA
	"ConnectToSupportHub", // [+] SSA
	"FileShareIncludeInFileList", // [+] SSA
	"FileShareReindexOnStart", // [+] SSA
	"SQLiteUseJournnalMemory", // [+] IRainman
	"SQLiteUseExclusiveLockMode", // [+] IRAINMAN_SQLITE_USE_EXCLUSIVE_LOCK_MODE
	"SecurityAskOnShareFromShell", // [+] SSA
	"PopupNewFolderShare", // [+] SSA
	"MaxFinishedUploads", "MaxFinishedDownloads", // [+] IRainman
	"PreviewServerPort", "PreviewServerSpeed", // [+] SSA
	"PreviewUseVideoScroll", "PreviewClientAutoStart", // [+] SSA
	"ProviderUseResources", // [+] SSA
	"ProviderUseMenu", "ProviderUseHublist", "ProviderUseLocations", // [+] SSA
	"AutoUpdateGeoIP", "AutoUpdateCustomLocation",
#ifdef SSA_SHELL_INTEGRATION
	"AutoUpdateShellExt", // [+] IRainman
#endif
#ifdef IRAINMAN_USE_BB_CODES
	"FormatBBCodeColors", // [+] SSA
#endif
#ifdef NIGHTORION_USE_STATISTICS_REQUEST
	"SettingsStatisticsAsk",
#endif
	"UseStatiscitcsSend",
	"ReportToUserIfOutdatedOsDetected20130523", // [+] IRainman https://code.google.com/p/flylinkdc/issues/detail?id=1032
	"SENTRY",
};

// [+] FlyLinkDC
static const string g_default_lang_file_name = "ru-RU.xml";
static const string g_english_lang_file_name = "en-US.xml";

SettingsManager::SettingsManager()
{
	//[+]IRainman
	BOOST_STATIC_ASSERT(_countof(settingTags) == SETTINGS_LAST + 1);// SettingsManager::SETTINGS_LAST and SettingsManager::settingTags[] size do not match ;) Check them out!
	// Thanks Boost developers for this wonderful functional
	
	for (size_t i = 0; i < SETTINGS_LAST; i++) //-V104
		isSet[i] = false;
		
	for (size_t i = 0; i < INT_LAST - INT_FIRST; i++) //-V104
	{
		intDefaults[i] = 0;
		intSettings[i] = 0;
	}
//	setDefaults(); // [-] IRainman calling in "startup" global function.
}

void SettingsManager::setDefaults()
{
#ifdef _DEBUG // [+] IRainman
	static bool l_isSettingsAlreadyInit = false;
	dcassert(!l_isSettingsAlreadyInit);
	if (l_isSettingsAlreadyInit)
		return;
	else
		l_isSettingsAlreadyInit = true;
#endif
	g_connectionSpeeds.reserve(15);
	g_connectionSpeeds.push_back("0.005");
	g_connectionSpeeds.push_back("0.01");
	g_connectionSpeeds.push_back("0.02");
	g_connectionSpeeds.push_back("0.05");
	g_connectionSpeeds.push_back("0.1");
	g_connectionSpeeds.push_back("0.2");
	g_connectionSpeeds.push_back("0.5");
	g_connectionSpeeds.push_back("1");
	g_connectionSpeeds.push_back("2");
	g_connectionSpeeds.push_back("5");
	g_connectionSpeeds.push_back("10");
	g_connectionSpeeds.push_back("20");
	g_connectionSpeeds.push_back("50");
	g_connectionSpeeds.push_back("100");
	g_connectionSpeeds.push_back("1000");
	dcassert(g_connectionSpeeds.size() == g_connectionSpeeds.capacity());
#ifdef PPA_INCLUDE_OLD_INNOSETUP_WIZARD
	const string l_dir = Util::getRegistryValueString("DownloadDir", true);
	if (!l_dir.empty())
		setDefault(DOWNLOAD_DIRECTORY, l_dir);
	else
#endif
		setDefault(DOWNLOAD_DIRECTORY, Util::getDownloadsPath());
	//setDefault(TEMP_DOWNLOAD_DIRECTORY, "");
	setDefault(SLOTS, 15); // [!] PPA 2->15
	setDefault(WEBSERVER_PORT, 0);
	setDefault(TCP_PORT, 0);
	setDefault(TLS_PORT, 0);
	setDefault(UDP_PORT, 0);
#ifdef STRONG_USE_DHT
	setDefault(DHT_PORT, 0);
#endif
	setDefault(AUTO_DETECT_CONNECTION, TRUE);// [!] IRainman default enable connection autodect
	setDefault(INCOMING_CONNECTIONS, INCOMING_FIREWALL_UPNP); // [!] IRainman default passive -> incoming firewall upnp
	setDefault(OUTGOING_CONNECTIONS, OUTGOING_DIRECT);
	//setDefault(INCOMING_AUTODETECT_FLAG, false);// [!] IRainman require special algorithm for choosing a random user can add additional checks.
//      setDefault(BAN_FEW_HUB, false); // [+] necros
#ifdef PPA_INCLUDE_AUTO_FOLLOW
	setDefault(AUTO_FOLLOW, TRUE);
#endif
	setDefault(CLEAR_SEARCH, TRUE);
	//setDefault(SHARE_HIDDEN, false);
	//setDefault(SHARE_SYSTEM, false); // [+] IRainman
	setDefault(SHARE_VIRTUAL, TRUE); // [+] IRainman
	setDefault(FILTER_MESSAGES, TRUE);
	//setDefault(MINIMIZE_TRAY, false); // [~] InfinitySky.
	setDefault(AUTO_SEARCH, TRUE);
	setDefault(TIME_STAMPS, TRUE);
	setDefault(CONFIRM_EXIT, TRUE);
	setDefault(POPUP_HUB_PMS, TRUE);
	setDefault(POPUP_BOT_PMS, TRUE);
	//setDefault(SUPPRESS_PMS, false); // [+] IRainman
	//setDefault(IGNORE_HUB_PMS, false);
	//setDefault(IGNORE_BOT_PMS, false);
	setDefault(BUFFER_SIZE_FOR_DOWNLOADS, 1024);
#ifdef IRAINMAN_ENABLE_HUB_LIST
	setDefault(HUBLIST_SERVERS, "http://dchublist.ru/hublist.xml.bz2;http://dchublist.com/hublist.xml.bz2;http://www.hublista.hu/hublist.xml.bz2;http://hublist.16mb.com/hublist.xml.bz2;");
#endif
	//setDefault(DOWNLOAD_SLOTS, 0); // [+] PPA
	//setDefault(MAX_DOWNLOAD_SPEED, 0);
	setDefault(LOG_DIRECTORY, Util::getLocalPath() + "Logs" PATH_SEPARATOR_STR);
	//setDefault(LOG_UPLOADS, false);
	//setDefault(LOG_DOWNLOADS, false);
	setDefault(LOG_PRIVATE_CHAT, TRUE); // [~] InfinitySky.
	setDefault(LOG_MAIN_CHAT, TRUE); // [~] InfinitySky.
	setDefault(LOG_IF_SUPPRESS_PMS, TRUE); // [+] IRainman
	setDefault(STATUS_IN_CHAT, TRUE);
	//setDefault(SHOW_JOINS, false);
	setDefault(UPLOAD_SPEED, g_connectionSpeeds[12]); // [+] PPA. 50m
	//setDefault(PRIVATE_MESSAGE_BEEP, false);
	//setDefault(PRIVATE_MESSAGE_BEEP_OPEN, false);
	setDefault(USE_SYSTEM_ICONS, TRUE);
	setDefault(POPUP_PMS, TRUE);
	//setDefault(MIN_UPLOAD_SPEED, 0);
// Параметры подверженные падению - провести дополнительную валиадцию
	setDefault(LOG_FORMAT_POST_DOWNLOAD, "%Y-%m-%d %H:%M:%S: %[target] " + STRING(DOWNLOADED_FROM) + " %[userNI] (%[userCID]), %[fileSI] (%[fileSIchunk]), %[speed], %[time]");
	setDefault(LOG_FORMAT_POST_UPLOAD, "%Y-%m-%d %H:%M:%S %[source] " + STRING(UPLOADED_TO) + " %[userNI] (%[userCID]), %[fileSI] (%[fileSIchunk]), %[speed], %[time]");
	setDefault(LOG_FORMAT_MAIN_CHAT, "[%Y-%m-%d %H:%M%:%S [extra]] %[message]");
	setDefault(LOG_FORMAT_PRIVATE_CHAT, "[%Y-%m-%d %H:%M%:%S [extra]] %[message]");
	setDefault(LOG_FORMAT_STATUS, "[%Y-%m-%d %H:%M:%S] %[message]");
	setDefault(LOG_FORMAT_SYSTEM, "[%Y-%m-%d %H:%M:%S] %[message]");
	setDefault(LOG_FORMAT_CUSTOM_LOCATION, "[%[line]] - %[error]"); // [+] IRainman
#ifdef RIP_USE_LOG_PROTOCOL
	setDefault(LOG_FORMAT_PROTOCOL, "[%Y-%m-%d %H:%M:%S] %[message]");
#endif
	setDefault(LOG_FILE_MAIN_CHAT, "%B - %Y\\%[hubURL].log");
	setDefault(LOG_FILE_STATUS, "%B - %Y\\%[hubURL]_status.log");
	setDefault(LOG_FILE_PRIVATE_CHAT, "PM\\%B - %Y\\%[userNI].log");
	setDefault(LOG_FILE_UPLOAD, "Uploads.log");
	setDefault(LOG_FILE_DOWNLOAD, "Downloads.log");
	setDefault(LOG_FILE_SYSTEM, "System.log");
	setDefault(LOG_FILE_WEBSERVER, "Webserver.log");
	setDefault(LOG_FILE_CUSTOM_LOCATION, "CustomLocation.log"); // [+] IRainman
#ifdef RIP_USE_LOG_PROTOCOL
	setDefault(LOG_FILE_PROTOCOL, "Protocol.log");
#endif
	setDefault(LOG_FILE_TRACE_SQLITE, "sqltrace.log");
	setDefault(LOG_FORMAT_TRACE_SQLITE, "[%Y-%m-%d %H:%M:%S] %[sql]");
	
	setDefault(LOG_FILE_DDOS_TRACE, "ddos.log");
	setDefault(LOG_FORMAT_DDOS_TRACE, "[%Y-%m-%d %H:%M:%S] %[message]");
	
	setDefault(LOG_FILE_DHT_TRACE, "dht.log");
	setDefault(LOG_FORMAT_DHT_TRACE, "[%Y-%m-%d %H:%M:%S] %[message]");
	
	setDefault(LOG_FILE_PSR_TRACE, "psr.log");
	setDefault(LOG_FORMAT_PSR_TRACE, "[%Y-%m-%d %H:%M:%S] %[message]");
	
	setDefault(TIME_STAMPS_FORMAT, "%X"); // [!] IRainman fix: use system format time. "%H:%M:%S"
//
	setDefault(URL_HANDLER, TRUE);
	//setDefault(AUTO_AWAY, false);
	setDefault(BIND_ADDRESS, "0.0.0.0");
	setDefault(SOCKS_PORT, 1080);
	setDefault(SOCKS_RESOLVE, 1);
	//setDefault(KEEP_LISTS, false);
	//setDefault(AUTO_KICK, false);
	setDefault(QUEUEFRAME_SHOW_TREE, TRUE);
	setDefault(QUEUEFRAME_SPLIT, 2500);
	setDefault(COMPRESS_TRANSFERS, TRUE);
	setDefault(SHOW_PROGRESS_BARS, TRUE);
	setDefault(DEFAULT_AWAY_MESSAGE, STRING(DEFAULT_AWAY_MESSAGE));
	setDefault(MAX_TAB_ROWS, 7); // [~] InfinitySky.
	setDefault(MAX_COMPRESSION, 9);
	setDefault(ANTI_FRAG, TRUE);
	//setDefault(ANTI_FRAG_MAX, 0);
	// [-] setDefault(NO_AWAYMSG_TO_BOTS, TRUE); [-] IRainman fix.
	//setDefault(SKIP_ZERO_BYTE, false);
	//setDefault(ADLS_BREAK_ON_FIRST, false);
	setDefault(HUB_USER_COMMANDS, TRUE);
#ifdef FLYLINKDC_HE
	setDefault(AUTO_SEARCH_AUTO_MATCH, TRUE);
#endif
	//setDefault(LOG_FILELIST_TRANSFERS, false);
	//setDefault(LOG_SYSTEM, false);
	//setDefault(SEND_UNKNOWN_COMMANDS, false);
	//setDefault(MAX_HASH_SPEED, 0);
#ifdef IRAINMAN_NTFS_STREAM_TTH
	setDefault(SAVE_TTH_IN_NTFS_FILESTREAM, TRUE);
	setDefault(SET_MIN_LENGHT_TTH_IN_NTFS_FILESTREAM, 16);// [+] NightOrion
#endif
	setDefault(FAV_SHOW_JOINS, TRUE); // [~] InfinitySky.
	//setDefault(LOG_STATUS_MESSAGES, false);
	
	setDefault(SHOW_TRANSFERVIEW, TRUE);
	setDefault(SHOW_TRANSFERVIEW_TOOLBAR, TRUE);
	
	setDefault(SHOW_STATUSBAR, TRUE);
	setDefault(SHOW_TOOLBAR, TRUE);
	//setDefault(POPUNDER_PM, false);
	//setDefault(POPUNDER_FILELIST, false);
	setDefault(MAGNET_REGISTER, TRUE);
	setDefault(MAGNET_ASK, TRUE);
	setDefault(MAGNET_ACTION, MAGNET_AUTO_SEARCH);
	//setDefault(DONT_DL_ALREADY_SHARED, false);
	setDefault(CONFIRM_OPEN_INET_HUBS, TRUE); // [+] InfinitySky.
	setDefault(CONFIRM_HUB_REMOVAL, TRUE);
	setDefault(CONFIRM_HUBGROUP_REMOVAL, TRUE); // [+] NightOrion
	setDefault(USE_CTRL_FOR_LINE_HISTORY, TRUE);
	//setDefault(JOIN_OPEN_NEW_WINDOW, false);
	setDefault(SHOW_LAST_LINES_LOG, 50); // [~] InfinitySky.
	setDefault(CONFIRM_DELETE, TRUE);
	//setDefault(ADVANCED_RESUME, TRUE); // [-] merge
	//setDefault(ADC_DEBUG, false);
	//setDefault(NMDC_DEBUG, false);
	setDefault(TOGGLE_ACTIVE_WINDOW, TRUE);
	setDefault(SEARCH_HISTORY, 10);
	setDefault(SET_MINISLOT_SIZE, 1024); // [+] PPA
	setDefault(PRIO_HIGHEST_SIZE, 64);
	setDefault(PRIO_HIGH_SIZE, 512); // [~] InfinitySky.
	setDefault(PRIO_NORMAL_SIZE, 1024); // [~] InfinitySky.
	setDefault(PRIO_LOW_SIZE, 2048); // [~] InfinitySky.
	//setDefault(PRIO_LOWEST, false);
	//setDefault(OPEN_PUBLIC, false);
	//setDefault(OPEN_FAVORITE_HUBS, false);
	//setDefault(OPEN_FAVORITE_USERS, false);
	//setDefault(OPEN_QUEUE, false);
	//setDefault(OPEN_FINISHED_DOWNLOADS, false);
	//setDefault(OPEN_FINISHED_UPLOADS, false);
	//setDefault(OPEN_SEARCH_SPY, false);
	//setDefault(OPEN_NETWORK_STATISTICS, false);
	//setDefault(OPEN_NOTEPAD, false);
	//setDefault(OPEN_WAITING_USERS, false);
	//setDefault(NO_IP_OVERRIDE, false);//[+] PPA [!]IRainman default disable ip override option
	// http://forum.wafl.ru/index.php?s=&showtopic=4300&view=findpost&p=82510
#ifdef FLYLINKDC_SUPPORT_WIN_XP
	setDefault(SOCKET_IN_BUFFER, MAX_SOCKET_BUFFER_SIZE);
	setDefault(SOCKET_OUT_BUFFER, MAX_SOCKET_BUFFER_SIZE);
	/*
	  // Increase the socket buffer sizes from the default sizes for WinXP.  In
	  // performance testing, there is substantial benefit by increasing from 8KB
	  // to 64KB.
	  // See also:
	  //    http://support.microsoft.com/kb/823764/EN-US
	  // On Vista, if we manually set these sizes, Vista turns off its receive
	  // window auto-tuning feature.
	  //    http://blogs.msdn.com/wndp/archive/2006/05/05/Winhec-blog-tcpip-2.aspx
	  // Since Vista's auto-tune is better than any static value we can could set,
	  // only change these on pre-vista machines.
	*/
#endif // FLYLINKDC_SUPPORT_WIN_XP
	setDefault(TLS_TRUSTED_CERTIFICATES_PATH, Util::getConfigPath() + "Certificates" PATH_SEPARATOR_STR);
	setDefault(TLS_PRIVATE_KEY_FILE, Util::getConfigPath() + "Certificates" PATH_SEPARATOR_STR "client.key");
	setDefault(TLS_CERTIFICATE_FILE, Util::getConfigPath() + "Certificates" PATH_SEPARATOR_STR "client.crt");
	setDefault(BOLD_FINISHED_DOWNLOADS, TRUE);
	setDefault(BOLD_FINISHED_UPLOADS, TRUE);
	setDefault(BOLD_QUEUE, TRUE);
	setDefault(BOLD_HUB, TRUE);
	setDefault(BOLD_PM, TRUE);
	setDefault(BOLD_SEARCH, TRUE);
	setDefault(BOLD_NEWRSS, TRUE);
	setDefault(BOLD_WAITING_USERS, TRUE);
	setDefault(NOTBOLD_FONT_ON_ACTIVITY_TAB, FALSE);
#ifdef FLYLINKDC_HE
	setDefault(AUTO_REFRESH_TIME, 360);
#else
	setDefault(AUTO_REFRESH_TIME, 60);
#endif
	setDefault(AUTO_SEARCH_LIMIT, 15);
	//setDefault(AUTO_KICK_NO_FAVS, false);
	setDefault(PROMPT_PASSWORD, TRUE);
	setDefault(SPY_FRAME_IGNORE_TTH_SEARCHES, FALSE);
	//setDefault(USE_TLS, false); //[+]PPA
	setDefault(MAX_COMMAND_LENGTH, 16 * 1024 * 1024);
	setDefault(ALLOW_UNTRUSTED_CLIENTS, TRUE);
	setDefault(ALLOW_UNTRUSTED_HUBS, TRUE);
	setDefault(FAST_HASH, TRUE);
	//setDefault(SORT_FAVUSERS_FIRST, false);
	//setDefault(SHOW_SHELL_MENU, false);
	setDefault(NUMBER_OF_SEGMENTS, 50); //[!]PPA
#ifndef FLYLINKDC_HE
	setDefault(SEGMENTS_MANUAL, TRUE); //[!]PPA
#endif
	//setDefault(HUB_SLOTS, 0);
	setDefault(TEXT_FONT, "Arial,-12,400,0"); // !SMT!-F [~] InfinitySky - бережём зрение.
	setDefault(DROP_MULTISOURCE_ONLY, TRUE);
#ifdef IRAINMAN_INCLUDE_DETECTION_MANAGER
	setDefault(PROFILES_URL, "https://dcaml.svn.sourceforge.net/svnroot/dcaml/"); // TODO - убрать
#endif
	setDefault(EXTRA_SLOTS, 10); //[+]PPA
	setDefault(SHUTDOWN_TIMEOUT, 150);
	//setDefault(SEARCH_PASSIVE, false); //[+] PPA
	//setDefault(MAX_UPLOAD_SPEED_LIMIT_NORMAL, 0);
	//setDefault(MAX_DOWNLOAD_SPEED_LIMIT_NORMAL, 0);
	//setDefault(MAX_UPLOAD_SPEED_LIMIT, 0);    // [~] brain-ripper, merge
	//setDefault(MAX_DOWNLOAD_SPEED_LIMIT, 0);  // [~] brain-ripper, merge
	//setDefault(MAX_UPLOAD_SPEED_LIMIT_TIME, 0);
	//setDefault(MAX_DOWNLOAD_SPEED_LIMIT_TIME, 0);
	setDefault(TOOLBAR, "1,27,-1,0,25,5,3,4,-1,6,7,8,9,22,-1,10,-1,14,23,-1,15,17,-1,19,21,29,24,28,20"); // [~] InfinitySky
	setDefault(WINAMPTOOLBAR, "0,-1,1,2,3,4,5,6,7,8"); // [+] Drakon.
//[-] PPA   setDefault(SEARCH_ALTERNATE_COLOUR, RGB(255,200,0));
	//setDefault(WEBSERVER, false);
	setDefault(LOG_FORMAT_WEBSERVER, "%Y-%m-%d %H:%M:%S %[ip] tried getting %[file]");
	//setDefault(LOG_WEBSERVER, false);
	setDefault(WEBSERVER_USER, "flylinkdcuser");
	setDefault(WEBSERVER_PASS, Util::getRandomNick());
	// [+] IRainman
	setDefault(WEBSERVER_POWER_USER, "flylinkdcadmin");
	setDefault(WEBSERVER_POWER_PASS, Util::getRandomNick());
	setDefault(WEBSERVER_SEARCHSIZE, 1000);
	setDefault(WEBSERVER_SEARCHPAGESIZE, 50);
	//setDefault(WEBSERVER_ALLOW_CHANGE_DOWNLOAD_DIR, false);
	//setDefault(WEBSERVER_ALLOW_UPNP, false);
	setDefault(WEBSERVER_BIND_ADDRESS, "0.0.0.0");
	// [~] IRainman
	setDefault(AUTO_PRIORITY_DEFAULT, TRUE);
	//setDefault(TOOLBARIMAGE, "");
	//setDefault(TOOLBARHOTIMAGE, "");
	//setDefault(TIME_DEPENDENT_THROTTLE, false);
	//setDefault(BANDWIDTH_LIMIT_START, 0);
	//setDefault(BANDWIDTH_LIMIT_END, 0);
	//setDefault(THROTTLE_ENABLE, false);
	setDefault(EXTRA_DOWNLOAD_SLOTS, 3);
	
	setDefault(BACKGROUND_COLOR, RGB(255, 255, 255));
	setDefault(TEXT_COLOR, RGB(67, 98, 154));
	
	setDefault(TEXT_GENERAL_BACK_COLOR, RGB(255, 255, 255));
	setDefault(TEXT_GENERAL_FORE_COLOR, RGB(67, 98, 154));
	//setDefault(TEXT_GENERAL_BOLD, false);
	//setDefault(TEXT_GENERAL_ITALIC, false);
	
	setDefault(TEXT_MYOWN_BACK_COLOR, RGB(255, 255, 255));
	setDefault(TEXT_MYOWN_FORE_COLOR, RGB(67, 98, 154));
	//setDefault(TEXT_MYOWN_BOLD, false);
	//setDefault(TEXT_MYOWN_ITALIC, false);
	
	setDefault(TEXT_PRIVATE_BACK_COLOR, RGB(255, 255, 255));
	setDefault(TEXT_PRIVATE_FORE_COLOR, RGB(67, 98, 154));
	//setDefault(TEXT_PRIVATE_BOLD, false);
	//setDefault(TEXT_PRIVATE_ITALIC, false); // [~] InfinitySky - Опция не работает.
	
	setDefault(TEXT_SYSTEM_BACK_COLOR, RGB(255, 255, 255));
	setDefault(TEXT_SYSTEM_FORE_COLOR, RGB(0, 128, 64));
	setDefault(TEXT_SYSTEM_BOLD, TRUE);
	//setDefault(TEXT_SYSTEM_ITALIC, false);
	
	setDefault(TEXT_SERVER_BACK_COLOR, RGB(255, 255, 255));
	setDefault(TEXT_SERVER_FORE_COLOR, RGB(0, 128, 64));
	setDefault(TEXT_SERVER_BOLD, TRUE);
	//setDefault(TEXT_SERVER_ITALIC, false);
	
	setDefault(TEXT_TIMESTAMP_BACK_COLOR, RGB(255, 255, 255));
	setDefault(TEXT_TIMESTAMP_FORE_COLOR, RGB(0, 91, 182)); // [~] InfinitySky
	//setDefault(TEXT_TIMESTAMP_BOLD, false);
	//setDefault(TEXT_TIMESTAMP_ITALIC, false);
	
	setDefault(TEXT_MYNICK_BACK_COLOR, RGB(240, 255, 240)); // [~] InfinitySky
	setDefault(TEXT_MYNICK_FORE_COLOR, RGB(0, 170, 0)); // [~] InfinitySky - более спокойные тона
	setDefault(TEXT_MYNICK_BOLD, TRUE);
	//setDefault(TEXT_MYNICK_ITALIC, false);
	
	setDefault(TEXT_FAV_BACK_COLOR, RGB(255, 255, 255));
	setDefault(TEXT_FAV_FORE_COLOR, RGB(0, 128, 255));  // [~] InfinitySky
	setDefault(TEXT_FAV_BOLD, TRUE);
	//setDefault(TEXT_FAV_ITALIC, false);
	
	setDefault(TEXT_OP_BACK_COLOR, RGB(255, 255, 255));
	setDefault(TEXT_OP_FORE_COLOR, RGB(0, 127, 0)); // [~] InfinitySky
	setDefault(TEXT_OP_BOLD, TRUE);
	//setDefault(TEXT_OP_ITALIC, false);
	
	setDefault(TEXT_URL_BACK_COLOR, RGB(255, 255, 255));
	setDefault(TEXT_URL_FORE_COLOR, RGB(0, 102, 204));
	//setDefault(TEXT_URL_BOLD, false);
	//setDefault(TEXT_URL_ITALIC, false);
	
	setDefault(TEXT_ENEMY_BACK_COLOR, RGB(244, 244, 244));
	setDefault(TEXT_ENEMY_FORE_COLOR, RGB(255, 165, 121));
	//setDefault(TEXT_ENEMY_BOLD, false);
	
	setDefault(BOLD_AUTHOR_MESS, TRUE);
	//setDefault(KICK_MSG_RECENT_01, "");
	//setDefault(KICK_MSG_RECENT_02, "");
	//setDefault(KICK_MSG_RECENT_03, "");
	//setDefault(KICK_MSG_RECENT_04, "");
	//setDefault(KICK_MSG_RECENT_05, "");
	//setDefault(KICK_MSG_RECENT_06, "");
	//setDefault(KICK_MSG_RECENT_07, "");
	//setDefault(KICK_MSG_RECENT_08, "");
	//setDefault(KICK_MSG_RECENT_09, "");
	//setDefault(KICK_MSG_RECENT_10, "");
	//setDefault(KICK_MSG_RECENT_11, "");
	//setDefault(KICK_MSG_RECENT_12, "");
	//setDefault(KICK_MSG_RECENT_13, "");
	//setDefault(KICK_MSG_RECENT_14, "");
	//setDefault(KICK_MSG_RECENT_15, "");
	//setDefault(KICK_MSG_RECENT_16, "");
	//setDefault(KICK_MSG_RECENT_17, "");
	//setDefault(KICK_MSG_RECENT_18, "");
	//setDefault(KICK_MSG_RECENT_19, "");
	//setDefault(KICK_MSG_RECENT_20, "");
	setDefault(PROGRESS_TEXT_COLOR_DOWN, RGB(0, 51, 0));
	setDefault(PROGRESS_TEXT_COLOR_UP, RGB(98, 0, 49));
	setDefault(SHOW_INFOTIPS, TRUE);
	//setDefault(MINIMIZE_ON_STARTUP, false);
	//setDefault(FREE_SLOTS_DEFAULT, false);
	setDefault(USE_EXTENSION_DOWNTO, TRUE);
	setDefault(ERROR_COLOR, RGB(255, 0, 0));
	setDefault(TRANSFER_SPLIT_SIZE, 8000);
	setDefault(DISCONNECT_SPEED, 5);
	setDefault(DISCONNECT_FILE_SPEED, 10);
	setDefault(DISCONNECT_TIME, 20);
	//setDefault(DISCONNECTING_ENABLE, false);
	setDefault(DISCONNECT_FILESIZE, 10);
	setDefault(REMOVE_SPEED, 2);
	//setDefault(FILE_SLOTS, 0); //[+]PPA
	setDefault(MENUBAR_TWO_COLORS, TRUE);
	setDefault(MENUBAR_LEFT_COLOR, RGB(0, 128, 255)); // [~] InfinitySky.
	setDefault(MENUBAR_RIGHT_COLOR, RGB(168, 211, 255)); // [~] InfinitySky.
	setDefault(MENUBAR_BUMPED, TRUE);
	
	setDefault(PERCENT_FAKE_SHARE_TOLERATED, 20);
	setDefault(REPORT_ALTERNATES, TRUE);
	
	//setDefault(SOUNDS_DISABLED, false);
	//setDefault(POPUPS_DISABLED, false); // [+] InfinitySky.
	setDefault(POPUPS_TABS_ENABLED, TRUE);  //[+] SCALOlaz
	setDefault(POPUPS_MESSAGEPANEL_ENABLED, TRUE);
	
	//setDefault(USE_12_HOUR_FORMAT, false); // [+] InfinitySky.
	setDefault(MINIMIZE_ON_CLOSE, TRUE); // [+] InfinitySky.
	setDefault(SHOW_CUSTOM_MINI_ICON_ON_TASKBAR, TRUE); // [+] InfinitySky.
	//setDefault(SHOW_CURRENT_SPEED_IN_TITLE, false); // [+] InfinitySky.
	//setDefault(CHECK_NEW_USERS, false);
	setDefault(UPLOADQUEUEFRAME_SHOW_TREE, TRUE);
	setDefault(UPLOADQUEUEFRAME_SPLIT, 2500);
	//setDefault(DONT_BEGIN_SEGMENT, false); // [!] PPA
	setDefault(DONT_BEGIN_SEGMENT_SPEED, 1024);
	
	//setDefault(DISCONNECT_RAW, 0);
	//setDefault(TIMEOUT_RAW, 0);
	//setDefault(FAKESHARE_RAW, 0);
	//setDefault(LISTLEN_MISMATCH, 0);
	//setDefault(FILELIST_TOO_SMALL, 0);
	//setDefault(FILELIST_UNAVAILABLE, 0);
	setDefault(DISPLAY_CHEATS_IN_MAIN_CHAT, TRUE);
	setDefault(AUTO_SEARCH_TIME, 1); //[.] PPA
	setDefault(REALTIME_QUEUE_UPDATE, TRUE);
	//setDefault(SUPPRESS_MAIN_CHAT, false);
	
	// default sounds
	setDefault(SOUND_BEGINFILE, Util::getSoundPath() + "DownloadBegins.wav");
	setDefault(SOUND_BEEPFILE, Util::getSoundPath() + "PrivateMessage.wav");
	setDefault(SOUND_FINISHFILE, Util::getSoundPath() + "DownloadFinished.wav");
	setDefault(SOUND_SOURCEFILE, Util::getSoundPath() + "AltSourceAdded.wav");
	setDefault(SOUND_UPLOADFILE, Util::getSoundPath() + "UploadFinished.wav");
	setDefault(SOUND_FAKERFILE, Util::getSoundPath() + "FakerFound.wav");
	setDefault(SOUND_CHATNAMEFILE, Util::getSoundPath() + "MyNickInMainChat.wav");
	setDefault(SOUND_TTH, Util::getSoundPath() + "FileCorrupted.wav");
	setDefault(SOUND_HUBCON, Util::getSoundPath() + "HubConnected.wav");
	setDefault(SOUND_HUBDISCON, Util::getSoundPath() + "HubDisconnected.wav");
	setDefault(SOUND_FAVUSER, Util::getSoundPath() + "FavUser.wav");
	setDefault(SOUND_FAVUSER_OFFLINE, Util::getSoundPath() + "FavUserDisconnected.wav"); // !SMT!-UI
	setDefault(SOUND_TYPING_NOTIFY, Util::getSoundPath() + "TypingNotify.wav");
	setDefault(SOUND_SEARCHSPY, Util::getSoundPath() + "SearchSpy.wav");
	
	//setDefault(POPUP_HUB_CONNECTED, false);
	//setDefault(POPUP_HUB_DISCONNECTED, false);
	setDefault(POPUP_FAVORITE_CONNECTED, TRUE);
	setDefault(POPUP_FAVORITE_DISCONNECTED, TRUE); // !SMT!-UI
	setDefault(POPUP_CHEATING_USER, TRUE);
	//setDefault(POPUP_CHAT_LINE, false); //[+]ProKK
	setDefault(POPUP_DOWNLOAD_START, TRUE);
	//setDefault(POPUP_DOWNLOAD_FAILED, false);
	setDefault(POPUP_DOWNLOAD_FINISHED, TRUE);
	//setDefault(POPUP_UPLOAD_FINISHED, false);
	//setDefault(POPUP_PM, false);
	setDefault(POPUP_NEW_PM, TRUE);
	setDefault(POPUP_SEARCH_SPY, FALSE);
	//setDefault(POPUP_TYPE, 0);
	//setDefault(POPUP_AWAY, false);
	setDefault(POPUP_MINIMIZED, TRUE);
	
	//setDefault(AWAY, false);
	//setDefault(SHUTDOWN_ACTION, 0);
	setDefault(MINIMUM_SEARCH_INTERVAL, 10);
	setDefault(PROGRESSBAR_ODC_STYLE, TRUE);
	
	setDefault(PROGRESS_3DDEPTH, 4); //-V112
	setDefault(PROGRESS_OVERRIDE_COLORS, TRUE);
	setDefault(PROGRESS_OVERRIDE_COLORS2, TRUE);
	setDefault(MAX_AUTO_MATCH_SOURCES, 5);
	setDefault(ENABLE_MULTI_CHUNK, TRUE);
	//setDefault(USERLIST_DBLCLICK, 0); // [~] brain-ripper: changed default value from GET_FILE_LIST to BROWSE_FILE_LIST // [~] InfinitySky: чтобы не было вопросов, почему не качается, как надо. Обратно BROWSE_FILE_LIST на GET_FILE_LIST
	//setDefault(TRANSFERLIST_DBLCLICK, 0);
	setDefault(CHAT_DBLCLICK, 1);
	setDefault(NORMAL_COLOUR, RGB(67, 98, 154));
	setDefault(RESERVED_SLOT_COLOR, RGB(255, 0, 128));
	setDefault(IGNORED_COLOR, RGB(192, 192, 192));
	setDefault(FAVORITE_COLOR, RGB(67, 98, 154));
	setDefault(FIREBALL_COLOR, RGB(255, 0, 0));
	setDefault(SERVER_COLOR, RGB(128, 128, 255));
	setDefault(PASIVE_COLOR, RGB(67, 98, 154));
	setDefault(OP_COLOR, RGB(0, 128, 64));
	setDefault(FULL_CHECKED_COLOUR, RGB(0, 160, 0));
	setDefault(BAD_CLIENT_COLOUR, RGB(204, 0, 0));
	setDefault(BAD_FILELIST_COLOUR, RGB(204, 0, 204));
	//setDefault(WINDOWS_STYLE_URL, TRUE);
	setDefault(HUBFRAME_VISIBLE, "1,1,0,1,1,1,1,1,1,1,1,1,1,1"); // [~] InfinitySky. Можно ещё донастроить: более значимые колонки помещаем вперёд, менее важные - назад. Также можно отрегулировать ширину колонок.
	setDefault(DIRECTORYLISTINGFRAME_VISIBLE, "1,1,1,1,1"); // [~] InfinitySky.
	setDefault(DIRECTORYLISTINGFRAME_SPLIT, 2500);
	setDefault(FINISHED_VISIBLE, "1,1,1,1,1,1,1,1");
	setDefault(FINISHED_UL_VISIBLE, "1,1,1,1,1,1,1");
	setDefault(ACCEPTED_DISCONNECTS, 5);
	setDefault(ACCEPTED_TIMEOUTS, 10);
	setDefault(EMOTICONS_FILE, "FlylinkSmilesInternational");
//	setDefault(FORGET_SEARCH_REQUEST, false);    // [+] SCALOlaz: do not save s-queue to dropbox list
	setDefault(SAVE_SEARCH_SETTINGS, TRUE); // [+] SCALOlaz: save selected type in search frame
	setDefault(SAVED_SEARCH_TYPE, 0);
	setDefault(SAVED_SEARCH_SIZEMODE, 2);
	setDefault(SAVED_SEARCH_MODE, 1);
//	setDefault(SAVED_SEARCH_SIZE, "");
	//setDefault(TABS_POS, 0); // [~] InfinitySky - 1) практичнее и удобнее; 2) понятнее новичкам (по схожести с браузерами и другими программами); 3) визуально проще заметить название на вкладке.
	setDefault(HUB_POSITION, POS_RIGHT); // [+] InfinitySky.
	setDefault(DOWNCONN_PER_SEC, 2);
	
	setDefault(HUBFRAME_COLUMNS_SORT, 0);   // COLUMN_NICK
	setDefault(HUBFRAME_COLUMNS_SORT_ASC, TRUE);
	// setDefault(HUBFRAME_ORDER, "0,1,4,2,5,6,7,8,9,3"); TODO: not working (
	// setDefault(HUBFRAME_WIDTHS, "1,1,0,1,0,0,0,0,0,1"); TODO: not working (
	
	setDefault(SEARCH_SPY_COLUMNS_SORT, 3); // COLUMN_TIME
	setDefault(SEARCH_SPY_COLUMNS_SORT_ASC, TRUE);
	
	setDefault(SEARCH_COLUMNS_SORT, 2); // COLUMN_HITS
	//setDefault(SEARCH_COLUMNS_SORT_ASC, false);
	
	setDefault(DIRLIST_COLUMNS_SORT, 0); // COLUMN_FILENAME
	setDefault(DIRLIST_COLUMNS_SORT_ASC, TRUE);
	
	setDefault(TRANSFERS_COLUMNS_SORT, 2); // COLUMN_FILENAME
	setDefault(TRANSFERS_COLUMNS_SORT_ASC, TRUE);
	// ApexDC++
#ifdef IRAINMAN_ENABLE_SLOTS_AND_LIMIT_IN_DESCRIPTION
	setDefault(ADD_TO_DESCRIPTION, TRUE);
	setDefault(ADD_DESCRIPTION_SLOTS, TRUE);
	setDefault(ADD_DESCRIPTION_LIMIT, TRUE);
#endif
	//setDefault(PROTECT_TRAY, false);
	//setDefault(PROTECT_START, false);
	//setDefault(PROTECT_CLOSE, false);
	//setDefault(STRIP_TOPIC, false);
	//setDefault(SKIPLIST_SHARE, Util::emptyString);
	setDefault(TB_IMAGE_SIZE, 24);  // [~] InfinitySky
	setDefault(TB_IMAGE_SIZE_HOT, 24);  // [~] InfinitySky
	//setDefault(SHOW_WINAMP_CONTROL, false);
	setDefault(USER_THERSHOLD, 1000);
	setDefault(PM_PREVIEW, TRUE);
	//setDefault(FILTER_ENTER, false);
	setDefault(HIGH_PRIO_FILES, "*.sfv;*.nfo;*sample*;*cover*;*.pls;*.m3u");
	//setDefault(LOW_PRIO_FILES, Util::emptyString);
	setDefault(POPUP_TIME, 5);
	setDefault(POPUP_W, 200);
	setDefault(POPUP_H, 90);
	setDefault(POPUP_TRANSP, 200);
	//setDefault(AWAY_START, 0);
	//setDefault(AWAY_END, 0);
	//setDefault(AWAY_TIME_THROTTLE, false);
	//setDefault(SECONDARY_AWAY_MESSAGE, Util::emptyString);
	setDefault(PROGRESSBAR_ODC_BUMPED, TRUE);
	setDefault(TOP_SPEED, 100);
	setDefault(TOP_UP_SPEED, 50);
	setDefault(STEALTHY_STYLE, TRUE);
	setDefault(STEALTHY_STYLE_ICO, TRUE);
	//setDefault(STEALTHY_STYLE_ICO_SPEEDIGNORE, false);
	setDefault(PSR_DELAY, 30);
	//setDefault(IP_IN_CHAT, false);
	//setDefault(COUNTRY_IN_CHAT, false);
	//setDefault(BROADCAST, 0);
	setDefault(REMEMBER_SETTINGS_PAGE, TRUE); // [~] InfinitySky.
	//setDefault(PAGE, 0);
	setDefault(REMEMBER_SETTINGS_WINDOW_POS, TRUE);
	setDefault(SETTINGS_WINDOW_POS_X, 0);
	setDefault(SETTINGS_WINDOW_POS_Y, 0);
	setDefault(SETTINGS_WINDOW_SIZE_X, 0);
	setDefault(SETTINGS_WINDOW_SIZE_YY, 0);
	//setDefault(SETTINGS_WINDOW_TRANSP, false);
	//setDefault(SETTINGS_WINDOW_COLORIZE, false);
	setDefault(SETTINGS_WINDOW_WIKIHELP, TRUE);
	setDefault(CHATBUFFERSIZE, 25000);
	setDefault(SEND_BLOOM, TRUE);
	setDefault(ENABLE_HUBMODE_PIC, TRUE);
	setDefault(ENABLE_COUNTRYFLAG, TRUE);
	setDefault(ENABLE_LAST_IP, TRUE);
	
	setDefault(ENABLE_FLY_SERVER, TRUE);
	setDefault(ENABLE_HIT_FILE_LIST, TRUE);
	setDefault(ENABLE_RATIO_USER_LIST, TRUE);
	
#ifdef FLYLINKDC_LOG_IN_SQLITE_BASE
	//setDefault(FLY_SQLITE_LOG, false); // [+] PPA
	setDefault(FLY_TEXT_LOG, !BOOLSETTING(FLY_SQLITE_LOG)); // [+] PPA
#endif //FLYLINKDC_LOG_IN_SQLITE_BASE
#ifdef IRAINMAN_INCLUDE_TEXT_FORMATTING
	//setDefault(FORMAT_BIU, false);
#endif
#ifdef IRAINMAN_USE_BB_CODES
	setDefault(FORMAT_BB_CODES, TRUE);// [+] IRainman
	setDefault(FORMAT_BB_CODES_COLORS, TRUE); // [+] SSA
#endif
	setDefault(FORMAT_BOT_MESSAGE, TRUE);// [+] IRainman
	//setDefault(PROT_USERS, Util::emptyString);
	//setDefault(MEDIA_PLAYER, 0);
	setDefault(WMP_FORMAT, "+me playing: '%[title]' at %[bitrate] (Windows Media Player %[version]) %[magnet]");    // [~] InfinitySky
	setDefault(ITUNES_FORMAT, "+me listens '%[artist] - %[title]' • listened to %[percent] (%[length], %[bitrate], iTunes %[version]) %[magnet]");  // [~] InfinitySky
	setDefault(MPLAYERC_FORMAT, "+me playing: '%[title]' (Media Player Classic) %[magnet]");    // [~] InfinitySky
	setDefault(JETAUDIO_FORMAT, "+me listens '%[artist] - %[title]' * listened to %[percent], JetAudio %[version] %[magnet]"); // [~] SSA
	setDefault(WINAMP_FORMAT, "+me listen: '%[artist] - %[track] - %[title]' • listened to %[percent] (%[length], %[bitrate], Winamp %[version]) %[magnet]");
	setDefault(QCDQMP_FORMAT, "+me listen in 'QCD/QMP %[version]': '%[title]' (%[bitrate],%[sample]) (%[elapsed] %[bar] %[length]) %[magnet]");
	//setDefault(IPUPDATE, false);//[+] PPA [!]IRainman default disable auto ip update
	//setDefault(IPUPDATE_INTERVAL, 0);//[!]IRainman default set ip update interval only startup
	//setDefault(PROT_FAVS, false);
	setDefault(RAW1_TEXT, "Raw 1");
	setDefault(RAW2_TEXT, "Raw 2");
	setDefault(RAW3_TEXT, "Raw 3");
	setDefault(RAW4_TEXT, "Raw 4");
	setDefault(RAW5_TEXT, "Raw 5");
	setDefault(MAX_MSG_LENGTH, 120);
	setDefault(POPUP_FONT, "Arial,-11,400,0"); // !SMT!-F
	setDefault(POPUP_TITLE_FONT, "Arial,-11,400,0"); // !SMT!-F
	setDefault(POPUP_BACKCOLOR, RGB(58, 122, 180));
	setDefault(POPUP_TEXTCOLOR, RGB(255, 255, 255));
	setDefault(POPUP_TITLE_TEXTCOLOR, RGB(255, 255, 255));
	//setDefault(POPUP_IMAGE, 0);
	setDefault(OPEN_LOGS_INTERNAL, TRUE); //[+]PPA
	// ApexDC++
#ifdef PPA_INCLUDE_DNS
	setDefault(NSLOOKUP_MODE, Socket::DNSCache::NSLOOKUP_DELAYED); // !SMT!-IP
	setDefault(NSLOOKUP_DELAY, 100); // !SMT!-IP
#endif
	// !SMT!-S
#ifdef IRAINMAN_ENABLE_AUTO_BAN
	//setDefault(ENABLE_AUTO_BAN, false);// [+]IRainman
#ifdef IRAINMAN_ENABLE_OP_VIP_MODE
	//setDefault(AUTOBAN_PPROTECT_OP, false);// [+]IRainman
#endif
	//setDefault(BAN_SLOTS, 0);
	//setDefault(BAN_SLOTS_H, 0);
	//setDefault(BAN_SHARE, 0);
	// old set(BAN_SHARE_MAX, 20);
	// old setDefault(BAN_SHARE_MAX, 20);
	//setDefault(BAN_LIMIT, 0);
	setDefault(BAN_MESSAGE, STRING(SETTINGS_BAN_MESSAGE));
	setDefault(BANMSG_PERIOD, 60);
	//setDefault(BAN_STEALTH, 0);
	//setDefault(BAN_FORCE_PM, 0);
	// old setDefault(BAN_SKIP_OPS, 1);
#endif // IRAINMAN_ENABLE_AUTO_BAN
	
	setDefault(SLOT_ASK, STRING(ASK_SLOT_TEMPLATE));
	//setDefault(EXTRASLOT_TO_DL, 0);
	
	// !SMT!-UI
	setDefault(BAN_COLOR, RGB(116, 154, 179));
	setDefault(DUPE_COLOR, RGB(115, 247, 230));
	setDefault(DUPE_EX1_COLOR, RGB(115, 247, 230));
	setDefault(DUPE_EX2_COLOR, RGB(115, 247, 230));//NSL
	//setDefault(MULTILINE_CHAT_INPUT, false);
	//setDefault(MULTILINE_CHAT_INPUT_BY_CTRL_ENTER, false);
	setDefault(SHOW_SEND_MESSAGE_BUTTON, TRUE);
	setDefault(SHOW_BBCODE_PANEL, TRUE);
	setDefault(SHOW_EMOTIONS_BTN, TRUE);
	setDefault(SHOW_MULTI_CHAT_BTN, TRUE);
#ifdef SCALOLAZ_CHAT_REFFERING_TO_NICK
# ifdef FLYLINKDC_HE
	setDefault(CHAT_REFFERING_TO_NICK, TRUE);  // [+] SCALOlaz
# endif
#endif
	setDefault(USE_AUTO_MULTI_CHAT_SWITCH, TRUE);
	setDefault(USE_MAGNETS_IN_PLAYERS_SPAM, TRUE);
	//setDefault(USE_BITRATE_FIX_FOR_SPAM, false);
	//setDefault(EXTERNAL_PREVIEW, false);
	//setDefault(SEND_SLOTGRANT_MSG, false);
	//setDefault(FAVUSERLIST_DBLCLICK, false);
	
	// !SMT!-PSW
	//setDefault(PROTECT_PRIVATE, 0);
	setDefault(PROTECT_PRIVATE_RND, 1);
	setDefault(PM_PASSWORD, Util::getRandomNick()); // Generate a random PM password
	setDefault(PM_PASSWORD_HINT, STRING(DEF_PASSWORD_HINT));
	setDefault(PM_PASSWORD_OK_HINT, STRING(DEF_PASSWORD_OK_HINT));
	//setDefault(PROTECT_PRIVATE_SAY, false);
	
	//[+]necros
	setDefault(COPY_WMLINK, "<a href=\"%[magnet]\" title=\"%[name]\" target=\"_blank\">%[name] (%[size])</a>");
	setDefault(URL_GET_IP, URL_GET_IP_DEFAULT); // [*] SkazochNik 19.01.2010
	
	//setDefault(UP_TRANSFER_COLORS, 0); // By Drakon // [~] InfinitySky. Отключено, так как нельзя поменять цвета.
	//setDefault(STARTUP_BACKUP, 0); // by Drakon
	//setDefault(GLOBAL_HUBFRAME_CONF, false);
	//setDefault(IGNORE_USE_REGEXP_OR_WC, false);
	//setDefault(STEALTHY_INDICATE_SPEEDS, false);
	setDefault(COLOUR_DUPE, RGB(0, 174, 87));
	//setDefault(NO_TTH_CHEAT, false);
	//setDefault(DELETE_CHECKED, false);
	//setDefault(TOPMOST, false);
	//setDefault(LOCK_TOOLBARS, false);
//	setDefault(AUTO_COMPLETE_SEARCH, TRUE);//[-]IRainman always is true!
	//setDefault(KEEP_DL_HISTORY, false);
	//setDefault(KEEP_UL_HISTORY, false);
	setDefault(SHOW_QUICK_SEARCH, TRUE);
	setDefault(SEARCH_DETECT_TTH, TRUE);
	//setDefault(FULL_FILELIST_NFO, false);
	setDefault(TABS_CLOSEBUTTONS, TRUE);
	//setDefault(TABS_CLOSEBUTTONS_ALT, false);
	//setDefault(VIEW_GRIDCONTROLS, false); // [+] ZagZag
	//setDefault(NON_HUBS_FRONT, false);
	setDefault(BLEND_OFFLINE_SEARCH, TRUE);
	setDefault(MAX_RESIZE_LINES, 1);
	//setDefault(USE_CUSTOM_LIST_BACKGROUND, false);
#ifdef _WIN32
	setDefault(MAIN_WINDOW_STATE, SW_SHOWNORMAL);
	setDefault(MAIN_WINDOW_SIZE_X, CW_USEDEFAULT);
	setDefault(MAIN_WINDOW_SIZE_Y, CW_USEDEFAULT);
	setDefault(MAIN_WINDOW_POS_X, CW_USEDEFAULT);
	setDefault(MAIN_WINDOW_POS_Y, CW_USEDEFAULT);
	setDefault(MDI_MAXIMIZED, TRUE);
	setDefault(UPLOAD_BAR_COLOR, RGB(0, 185, 0)); // [~] InfinitySky.
	setDefault(DOWNLOAD_BAR_COLOR, RGB(0, 128, 255)); // [~] InfinitySky.
	setDefault(PROGRESS_BACK_COLOR, RGB(95, 95, 95));
	setDefault(PROGRESS_COMPRESS_COLOR, RGB(222, 160, 0));
	setDefault(PROGRESS_SEGMENT_COLOR, RGB(49, 106, 197));
	setDefault(COLOR_RUNNING, RGB(0, 0, 100));
	setDefault(COLOR_DOWNLOADED, RGB(255, 255, 100));
	setDefault(COLOR_VERIFIED, RGB(0, 255, 0));
	setDefault(TAB_SELECTED_COLOR, RGB(106, 181, 255));
	setDefault(TAB_OFFLINE_COLOR, RGB(255, 148, 106));
	setDefault(TAB_ACTIVITY_COLOR, RGB(147, 202, 0));
	setDefault(TAB_SELECTED_TEXT_COLOR, RGB(0, 100, 121));  //[+] SCALOlaz [~] Sergey Shuhskanov
	setDefault(TAB_OFFLINE_TEXT_COLOR, RGB(159, 64, 0));    //[+]
	setDefault(TAB_ACTIVITY_TEXT_COLOR, RGB(0, 128, 0));    //[+]
#ifdef SCALOLAZ_USE_COLOR_HUB_IN_FAV
	setDefault(HUB_IN_FAV_BK_COLOR, RGB(191, 180, 26));
	setDefault(HUB_IN_FAV_CONNECT_BK_COLOR, RGB(191, 236, 26));
#endif
	//make sure the total of the following and PROGRESS_BACK_COLOR are under 255,255,255, since they are added together
	setDefault(COLOR_AVOIDING, RGB(100, 0, 0));
	
	// [+] brain-ripper
	// Animated smiles.
	setDefault(CHAT_ANIM_SMILES, TRUE);
	setDefault(SMILE_SELECT_WND_ANIM_SMILES, TRUE);
	
	//setDefault(USE_OLD_SHARING_UI, false);
#endif
//[+] WhiteD. Custom ratio message
	setDefault(RATIO_TEMPLATE, "/me ratio: %[ratio] (Uploaded: %[up] | Downloaded: %[down])");
#ifdef RIP_USE_PORTAL_BROWSER
	//setDefault(PORTAL_BROWSER_UPDATE_URL, "");
#endif
	
//[+} SSA: Custom menu
	//setDefault(USE_CUSTOM_MENU, false);
	setDefault(CUSTOM_MENU_PATH, "file://./Settings/custom_menu.xml");
//[+] SSA: RSS Auto update
	setDefault(RSS_AUTO_REFRESH_TIME, 1);
	//setDefault(URL_IPTRUST, "");
// End of addition.

	setDefault(OVERLAP_CHUNKS, TRUE);
	// [!] SSA - r7122 - seems fixed setDefault(KEEP_FINISHED_FILES_OPTION, TRUE); // [+] IRainman set to enable default, it's workaraund to fix application freezes then remove download from queue after fineshed. :) I love You World!
	setDefault(EXTRA_PARTIAL_SLOTS, 1);
	setDefault(AUTO_SLOTS, 5);
	setDefault(ALLOW_NAT_TRAVERSAL, TRUE);
	//setDefault(USE_EXPLORER_THEME, false); // [~] IRainman set to disable default.
#ifdef FLYLINKDC_HE
	setDefault(USE_DHT, TRUE);
	setDefault(USE_DHT_NOTANSWER, TRUE);
#endif
	setDefault(LANGUAGE_FILE, g_default_lang_file_name);
	setDefault(MIN_MULTI_CHUNK_SIZE, 2); // [+] IRainman
	setDefault(MIN_MEDIAINFO_SIZE, 1); // [+] PPA
	setDefault(SHOW_SEEKERS_IN_SPY_FRAME, TRUE); // [+] IRainman
#ifdef FLYLINKDC_HE
	setDefault(REDUCE_PRIORITY_IF_MINIMIZED_TO_TRAY, TRUE); // [+] IRainman
#endif
	setDefault(ON_DOWNLOAD_SETTING, ON_DOWNLOAD_ASK); //[+] SSA
	// [+] SSA - AutoUpdate
	//setDefault(AUTOUPDATE_UPDATE_UNKNOWN, false); // [+] SSA
	setDefault(AUTOUPDATE_EXE, TRUE); // [+] SSA
	setDefault(AUTOUPDATE_UTILITIES, TRUE); // [+] SSA
	setDefault(AUTOUPDATE_LANG, TRUE); // [+] SSA
	setDefault(AUTOUPDATE_PORTALBROWSER, TRUE); // [+] SSA
	setDefault(AUTOUPDATE_EMOPACKS, TRUE); // [+] SSA
	setDefault(AUTOUPDATE_WEBSERVER, TRUE); // [+] SSA
	// setDefault(AUTOUPDATE_TIME,false); // [+] SSA
	setDefault(AUTOUPDATE_SHOWUPDATEREADY, TRUE); // [+] SSA
	setDefault(AUTOUPDATE_RUNONSTARTUP, TRUE); // [+] SSA
	setDefault(AUTOUPDATE_SOUNDS, TRUE); // [+] SSA
	setDefault(AUTOUPDATE_ICONTHEMES, TRUE); // [+] SSA
	setDefault(AUTOUPDATE_COLORTHEMES, TRUE); // [+] SSA
	setDefault(AUTOUPDATE_DOCUMENTATION, TRUE); // [+] SSA
	setDefault(AUTOUPDATE_UPDATE_CHATBOT, TRUE); // [+] SSA
	setDefault(AUTOUPDATE_GEOIP, TRUE); // [+] IRainman
	setDefault(AUTOUPDATE_CUSTOMLOCATION, TRUE); // [+] IRainman
#ifdef SSA_SHELL_INTEGRATION
	setDefault(AUTOUPDATE_SHELL_EXT, TRUE); // [+] IRainman
#endif
	//setDefault(AUTOUPDATE_FORCE_RESTART, false); // [+] SSA
	setDefault(AUTOUPDATE_ENABLE, TRUE); // [+] SSA
	//setDefault(AUTOUPDATE_USE_CUSTOM_URL, false); //[+] SSA
	
	//setDefault(EXTRA_SLOT_BY_IP, false); // [+] SSA
	//setDefault(EXTRA_SLOT_FOR_MY_NET, false); // [+] SSA
	setDefault(DCLST_REGISTER, TRUE); // [+] IRainman dclst support
	setDefault(DCLST_CREATE_IN_SAME_FOLDER, TRUE); // [+] SSA
	setDefault(DCLST_ASK, TRUE); // [+] SSA
	setDefault(DCLST_ACTION, MAGNET_AUTO_SEARCH); // [+] SSA
	setDefault(DCLST_INCLUDESELF, TRUE); // [+] SSA
	// [+] SSA - update to beta version
#if defined(FLYLINKDC_BETA)
	setDefault(AUTOUPDATE_TO_BETA, TRUE);
#else
	setDefault(AUTOUPDATE_TO_BETA, false);
#endif
#ifdef FLYLINKDC_HE
	setDefault(CONNECT_TO_SUPPORT_HUB, TRUE); // [+] SSA - maybe set to true in EU version?
#endif
	setDefault(FILESHARE_INC_FILELIST, TRUE); // [+] SSA
	setDefault(FILESHARE_REINDEX_ON_START, TRUE); // [+] SSA
	//setDefault(SQLITE_USE_JOURNAL_MEMORY, false); // [+] IRainman
	
	// IRAINMAN_SQLITE_USE_EXCLUSIVE_LOCK_MODE
	// Dear developers after the funeral FlylinkDiscover of users using an external editor base is extremely small. And who manages to use it are able to remove this check.
	setDefault(SQLITE_USE_EXCLUSIVE_LOCK_MODE, TRUE);
	// ~IRAINMAN_SQLITE_USE_EXCLUSIVE_LOCK_MODE
	setDefault(SECURITY_ASK_ON_SHARE_FROM_SHELL, TRUE); // [+] SSA
	setDefault(POPUP_NEW_FOLDERSHARE, TRUE); // [+] SSA
	setDefault(MAX_FINISHED_UPLOADS, 1000); // [+] IRainman
	setDefault(MAX_FINISHED_DOWNLOADS, 1000); // [+] IRainman
	// Preview
	setDefault(INT_PREVIEW_SERVER_PORT, 550); // [+] SSA
	setDefault(INT_PREVIEW_SERVER_SPEED, 500); // [+] SSA
	// setDefault(INT_PREVIEW_CLIENT_PATH, "C:\\Program Files\\VideoLAN\\VLC\\vlc.exe"); // [+] SSA
	setDefault(INT_PREVIEW_CLIENT_PATH, "C:\\Program Files\\SMPlayer\\smplayer.exe");  // [+] SSA
	setDefault(INT_PREVIEW_USE_VIDEO_SCROLL, TRUE);
	setDefault(INT_PREVIEW_START_CLIENT, TRUE);
#ifdef NIGHTORION_USE_STATISTICS_REQUEST
	setDefault(SETTINGS_STATISTICS_ASK, TRUE);
#endif
	setDefault(USE_FLY_SERVER_STATICTICS_SEND, TRUE);
#ifdef FLYLINKDC_USE_CHECK_OLD_OS
	setDefault(REPORT_TO_USER_IF_OUTDATED_OS_DETECTED, TRUE); // [+] IRainman https://code.google.com/p/flylinkdc/issues/detail?id=1032
#endif
#ifdef FLYLINKDC_HE
	setDefault(AUTOUPDATE_USE_CUSTOM_URL, TRUE);
	setDefault(AUTOUPDATE_SERVER_URL, "http://studia2000.sytes.net/flyupdate"); // TODO: move to google.
	importDctheme(Text::toT(Util::getThemesPath()) + _T("Green_Orange.dctheme"), true);
# ifdef _WIN64
	setDefault(THEME_MANAGER_THEME_DLL_NAME, "resourceGO_x64.dll");
# else
	setDefault(THEME_MANAGER_THEME_DLL_NAME, "resourceGO.dll");
# endif
#endif // FLYLINKDC_HE
	
	setSearchTypeDefaults();
	// TODO - грузить это из сети и отложенно когда понадобится.
	// http://code.google.com/p/flylinkdc/issues/detail?id=1279
	Util::shrink_to_fit(&strDefaults[STR_FIRST], &strDefaults[STR_LAST]); // [+] IRainman opt.
}

bool SettingsManager::LoadLanguage()
{
	return ResourceManager::loadLanguage(Util::getLocalisationPath() + get(LANGUAGE_FILE));
}

void SettingsManager::load(const string& aFileName)
{
	try
	{
		SimpleXML xml;
		xml.fromXML(File(aFileName, File::READ, File::OPEN).read());
		xml.stepIn();
		if (xml.findChild("Settings"))
		{
			xml.stepIn();
			int i;
			for (i = STR_FIRST; i < STR_LAST; i++)
			{
				const string& attr = settingTags[i];
				dcassert(attr.find("SENTRY") == string::npos);
				
				if (xml.findChild(attr))
					set(StrSetting(i), xml.getChildData());
				xml.resetCurrentChild();
			}
			for (i = INT_FIRST; i < INT_LAST; i++)
			{
				const string& attr = settingTags[i];
				dcassert(attr.find("SENTRY") == string::npos);
				
				if (xml.findChild(attr))
					set(IntSetting(i), Util::toInt(xml.getChildData()));
				xml.resetCurrentChild();
			}
			xml.stepOut();
		}
		xml.resetCurrentChild();
		if (xml.findChild("SearchTypes"))
		{
			try
			{
				g_searchTypes.clear();
				xml.stepIn();
				while (xml.findChild("SearchType"))
				{
					const string& extensions = xml.getChildData();
					if (extensions.empty())
					{
						continue;
					}
					const string& name = xml.getChildAttrib("Id");
					if (name.empty())
					{
						continue;
					}
					g_searchTypes[name] = Util::splitSettingAndLower(extensions);
				}
				xml.stepOut();
			}
			catch (const SimpleXMLException&)
			{
				setSearchTypeDefaults();
			}
		}
		xml.stepOut();
	}
	catch (const FileException&)
	{
		dcassert(0);
	}
	catch (const SimpleXMLException&) // TODO Битый конфиг XML https://crash-server.com/Problem.aspx?ProblemID=15638
	{
		dcassert(0);
	}
	
	if (SETTING(TCP_PORT) == 0)
	{
		set(WEBSERVER_PORT, getNewPortValue(0));
		set(TCP_PORT, getNewPortValue(get(WEBSERVER_PORT)));
		set(TLS_PORT, getNewPortValue(get(TCP_PORT)));
		set(UDP_PORT, get(TCP_PORT));
#ifdef STRONG_USE_DHT
		set(DHT_PORT, getNewPortValue(get(UDP_PORT)));
#endif
	}
	
	if (SETTING(PRIVATE_ID).length() != 39 || CID(SETTING(PRIVATE_ID)).isZero())
		set(PRIVATE_ID, CID::generate().toBase32());
#ifdef STRONG_USE_DHT
	if (SETTING(DHT_KEY).length() != 39 || CID(SETTING(DHT_KEY)).isZero())
		set(DHT_KEY, CID::generate().toBase32());
#endif
		
	const string l_config_version = get(CONFIG_VERSION, false);
	const int l_version = Util::toInt(l_config_version); // [!] FlylinkDC used revision version (not kernel version!)
	
	if (l_version && l_version <= 5)
	{
	
		// Formats changed, might as well remove these...
		set(LOG_FORMAT_POST_DOWNLOAD, Util::emptyString);
		set(LOG_FORMAT_POST_UPLOAD, Util::emptyString);
		set(LOG_FORMAT_MAIN_CHAT, Util::emptyString);
		set(LOG_FORMAT_PRIVATE_CHAT, Util::emptyString);
		set(LOG_FORMAT_STATUS, Util::emptyString);
		set(LOG_FORMAT_SYSTEM, Util::emptyString);
#ifdef RIP_USE_LOG_PROTOCOL
		set(LOG_FORMAT_PROTOCOL, Util::emptyString);
#endif
		set(LOG_FILE_MAIN_CHAT, Util::emptyString);
		set(LOG_FILE_STATUS, Util::emptyString);
		set(LOG_FILE_PRIVATE_CHAT, Util::emptyString);
		set(LOG_FILE_UPLOAD, Util::emptyString);
		set(LOG_FILE_DOWNLOAD, Util::emptyString);
		set(LOG_FILE_SYSTEM, Util::emptyString);
#ifdef RIP_USE_LOG_PROTOCOL
		set(LOG_FILE_PROTOCOL, Util::emptyString);
#endif
	}
	
	const string& languageFile = get(LANGUAGE_FILE);
	if (languageFile.empty() || l_version <= 7589 || !File::isExist(Util::getLocalisationPath() + languageFile))// [+] FlylinkDC
	{
		auto getLanguageFileFromOldName = [languageFile]() -> string
		{
			if (languageFile == "Russian.xml" || // For r4xx version, sorry but the Ukrainian and Belarusian will be replaced by Russian sweat going, alternatives to the old branch unfortunately no :(
			languageFile == "RUS.xml")// For first r500 alpha and beta
			{
				return g_default_lang_file_name;
			}
			else if (languageFile == "BEL.xml")
			{
				return "be-BY.xml";
			}
			else if (languageFile == "UKR.xml")
			{
				return "uk-UA.xml";
			}
			else // For migration from "ENG.xml" and all others erroneous language file names and to help you migrate from other clients.
			{
				return g_english_lang_file_name;
			}
		};
		// [!] IRainman:
		// The latest revision of the old names of the localization files http://code.google.com/p/flylinkdc/source/detail?r=7589
		HKEY hk = nullptr;
		if (::RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\") _T(APPNAME)
#ifdef _WIN64
		                   _T(" x64_is1")
#else
		                   _T("_is1")
#endif
		                   , 0, KEY_READ, &hk) == ERROR_SUCCESS)
		{
			TCHAR l_buf[32];
			l_buf[0] = 0;
			DWORD l_bufLen = sizeof(l_buf);
			const wchar_t* l_key = _T("Inno Setup: Language");
			::RegQueryValueEx(hk, l_key, NULL, NULL, (LPBYTE)l_buf, &l_bufLen);
			::RegCloseKey(hk);
			if (l_bufLen)
			{
				string l_result = Text::fromT(l_buf);
				boost::replace_all(l_result, "_", "-");
				l_result += ".xml";
				if (File::isExist(Util::getLocalisationPath() + l_result))
					set(LANGUAGE_FILE, l_result);
			}
			else
				set(LANGUAGE_FILE, getLanguageFileFromOldName());
		}
		else
			set(LANGUAGE_FILE, getLanguageFileFromOldName());
	}
#ifdef IRAINMAN_INCLUDE_PROVIDER_RESOURCES_AND_CUSTOM_MENU
	{
		const string l_file = Util::getConfigPath() + "ISP-root.url";
		std::fstream l_isp_url_file(l_file, std::ios_base::in);
		if (l_isp_url_file.is_open())
		{
			std::string  l_url;
			std::getline(l_isp_url_file, l_url);
			if (Util::isHttpLink(l_url))
			{
				set(ISP_RESOURCE_ROOT_URL, l_url);
				set(PROVIDER_USE_RESOURCES, TRUE);
				set(PROVIDER_USE_MENU, TRUE);
				set(PROVIDER_USE_HUBLIST, TRUE);
				set(PROVIDER_USE_PROVIDER_LOCATIONS, TRUE);
				l_isp_url_file.close();
				File::deleteFile(l_file);
			}
		}
	}
#endif
	string l_GET_IP = SETTING(URL_GET_IP);
	boost::replace_all(l_GET_IP, "flylinkdc.ru/", "flylinkdc.com/");
	set(URL_GET_IP, l_GET_IP);
	
	string l_result = SETTING(PM_PASSWORD_HINT);
	auto i = l_result.find("flyfromsky");
	if (i != string::npos)
	{
		l_result.replace(i, 10, "%[pm_pass]");
		set(PM_PASSWORD_HINT, l_result);
	}
	i = l_result.find("rabbit");
	if (i != string::npos)
	{
		l_result.replace(i, 6, "%[pm_pass]");
		set(PM_PASSWORD_HINT, l_result);
	}
	i = l_result.find("skaryna");
	if (i != string::npos)
	{
		l_result.replace(i, 7, "%[pm_pass]");
		set(PM_PASSWORD_HINT, l_result);
	}
	//  set(PM_PASSWORD_HINT, STRING(DEF_PASSWORD_HINT));    //Жёстко заменить всю строку.
#ifdef HOURLY_CHECK_UPDATE
	set(AUTOUPDATE_TIME, 24);
#endif // HOURLY_CHECK_UPDATE
	
	//удалить через несколько релизов
	if (strstr(get(TEMP_DOWNLOAD_DIRECTORY).c_str(), "%[targetdir]\\") != 0)
		set(TEMP_DOWNLOAD_DIRECTORY, "");
		
	const auto& l_path = SETTING(TLS_TRUSTED_CERTIFICATES_PATH);
	if (!l_path.empty())
	{
		File::ensureDirectory(l_path);
	}
}

void SettingsManager::loadOtherSettings()
{
	try
	{
		SimpleXML xml;
		xml.fromXML(File(getConfigFile(), File::READ, File::OPEN).read());
		xml.stepIn();
		fire(SettingsManagerListener::Load(), xml);
		xml.stepOut();
	}
	catch (const FileException&)
	{
		dcassert(0);
	}
	catch (const SimpleXMLException&) // TODO Битый конфиг XML https://crash-server.com/Problem.aspx?ProblemID=15638
	{
		dcassert(0);
	}
	
}

bool SettingsManager::set(StrSetting key, const string& value)
{
	// [!] Please do not initialize l_auto because when you make changes, you can quietly break logic,
	// and so though the compiler will issue a warning of a potentially uninitialized variable.
	bool l_auto; // [+] IRainman
	// [!]
	
	switch (key)
	{
			// [+] IRainman fix.
#define REDUCE_LENGHT(maxLenght)\
	{\
		l_auto = value.size() > maxLenght;\
		strSettings[key - STR_FIRST] = l_auto ? value.substr(0, maxLenght) : value;\
	}
			
#define REPLACE_SPACES()\
	{\
		auto cleanValue = value;\
		boost::replace_all(cleanValue, " ", "");\
		strSettings[key - STR_FIRST] = cleanValue;\
		l_auto = false;\
	}
			// [~] IRainman fix.
			
		case LOG_FORMAT_POST_DOWNLOAD:
		case LOG_FORMAT_POST_UPLOAD:
		case LOG_FORMAT_MAIN_CHAT:
		case LOG_FORMAT_PRIVATE_CHAT:
		case LOG_FORMAT_STATUS:
		case LOG_FORMAT_SYSTEM:
		case LOG_FORMAT_CUSTOM_LOCATION:
		case LOG_FORMAT_TRACE_SQLITE:
		case LOG_FORMAT_DDOS_TRACE:
#ifdef RIP_USE_LOG_PROTOCOL
		case LOG_FORMAT_PROTOCOL:
#endif
		case LOG_FILE_MAIN_CHAT:
		case LOG_FILE_STATUS:
		case LOG_FILE_PRIVATE_CHAT:
		case LOG_FILE_UPLOAD:
		case LOG_FILE_DOWNLOAD:
		case LOG_FILE_SYSTEM:
		case LOG_FILE_WEBSERVER:
		case LOG_FILE_CUSTOM_LOCATION:
		case LOG_FILE_TRACE_SQLITE:
		case LOG_FILE_DDOS_TRACE:
		case LOG_FILE_DHT_TRACE:
		case LOG_FILE_PSR_TRACE:
#ifdef RIP_USE_LOG_PROTOCOL
		case LOG_FILE_PROTOCOL:
#endif
		case TIME_STAMPS_FORMAT:
		{
			string l_new_value = value;
			l_auto = Text::safe_strftime_translate(l_new_value);
			strSettings[key - STR_FIRST] = l_new_value;
		}
		break;
		case BIND_ADDRESS:
		case WEBSERVER_BIND_ADDRESS:
			// [+] IRainman fix.
		case URL_GET_IP:
		case PROFILES_URL:
		case PORTAL_BROWSER_UPDATE_URL:
		case URL_IPTRUST:
		case AUTOUPDATE_SERVER_URL: //[PVS-Studio] V556 The values of different enum types are compared: key == AUTOUPDATE_USE_CUSTOM_URL. settingsmanager.cpp 1406
		case ISP_RESOURCE_ROOT_URL:
			// [~] IRainman fix.
		{
			string l_trim_val = value;
			boost::algorithm::trim(l_trim_val);
			strSettings[key - STR_FIRST] = l_trim_val;
			l_auto = false; // [+] IRainman
		}
		break;
		// [+] IRainamn opt.
		case PROT_USERS:
		{
			REPLACE_SPACES();
			getInstance()->fire(SettingsManagerListener::UsersChanges());
		}
		break;
		case LOW_PRIO_FILES:
		case HIGH_PRIO_FILES:
		{
			REPLACE_SPACES();
			getInstance()->fire(SettingsManagerListener::QueueChanges());
		}
		break;
		case SKIPLIST_SHARE:
		{
			REPLACE_SPACES();
			getInstance()->fire(SettingsManagerListener::ShareChanges());
		}
		break;
		// [~] IRainamn opt.
		case NICK:
			REDUCE_LENGHT(49); // [~] InfinitySky. 35 -> 49
			break;
		case DESCRIPTION:
			REDUCE_LENGHT(100); // [~] InfinitySky. 50 -> 100
			break;
		case EMAIL:
			REDUCE_LENGHT(64);
			break;
		case DCLST_FOLDER_DIR:
		case DOWNLOAD_DIRECTORY:
		case TEMP_DOWNLOAD_DIRECTORY:
		case LOG_DIRECTORY:
		case AUTOUPDATE_PATH_WITH_UPDATE: // [+] IRainman fix: please check user input data only if change.
		{
			string& l_path = strSettings[key - STR_FIRST];
			l_path = value;
			AppendPathSeparator(l_path);
			l_auto = false;
		}
		break;
		default:
			strSettings[key - STR_FIRST] = value;
			l_auto = false ; // [+] IRainman
			break;
			
#undef REDUCE_LENGHT
	}
	
	if (l_auto) // Если параметр изменили в момент загрузки - ставим маркер что нужно записаться обратно в файл.
	{
		isSet[key] = true;
	}
	else if (key >= SOUND_BEEPFILE && key <= SOUND_SEARCHSPY)
	{
		isSet[key] = true;
	}
	else
	{
		isSet[key] = !value.empty();
	}
	
	return l_auto; // [+] IRainman
}

bool SettingsManager::set(IntSetting key, int value)
{
	bool l_auto = false; // [+] IRainman
	switch (key)
	{
//[+] IRainman
#define VER_MIN(min)\
	if (value < min)\
	{\
		value = min;\
		l_auto = true;\
	}
#define VER_MAX(max)\
	if (value > max)\
	{\
		value = max;\
		l_auto = true;\
	}
#define VERIFI(min, max)\
	VER_MIN(min);\
	VER_MAX(max)
#define VER_MIN_EXCL_ZERO(min)\
	if (value < min && value != 0)\
	{\
		value = min;\
		l_auto = true;\
	}
#define GET_NEW_PORT_VALUE_IF_CONFLICTS(conflict_key)\
	{\
		const int l_current_conflicts_port = get(conflict_key,false);\
		if (value == l_current_conflicts_port)\
		{\
			value = getNewPortValue(l_current_conflicts_port);\
			l_auto = true;\
		}\
	}
//[~] IRainman
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		case BAN_SHARE:
		{
			//[!] IRainman when you run a second copy of the application
			// should not create two copies ShareManager
			int maxBanShare = 20;
			if (ShareManager::isValidInstance())
			{
				maxBanShare = min(maxBanShare, static_cast<int>(ShareManager::getInstance()->getSharedSize() / static_cast<int64_t>(1024 * 1024 * 1024)));
			}
			VERIFI(0, maxBanShare);
			break;
		}
		case BAN_LIMIT:
		{
			VERIFI(0, 50);
			break;
		}
		case BAN_SLOTS:
		{
			VERIFI(0, 5);
			break;
		}
		case BAN_SLOTS_H:
		{
			VER_MIN_EXCL_ZERO(10);
			break;
		}
#endif // IRAINMAN_ENABLE_AUTO_BAN
		case AUTO_SEARCH_LIMIT:
		{
			VER_MIN(1);
			break;
		}
		case BUFFER_SIZE_FOR_DOWNLOADS:
		{
			VERIFI(0, 1024 * 1024);
			break;
		}
		case SLOTS: //[+]PPA
		{
			VERIFI(1, 500);
			break;
		}
		case EXTRA_SLOTS:
		{
			VER_MIN(3);
			break;
		}
		case HUB_SLOTS:
		{
			VER_MIN(0);
			break;
		}
		case SET_MINISLOT_SIZE:
		{
			VER_MIN(16);
			break;
		}
#ifdef FLYLINKDC_SUPPORT_WIN_XP
		case SOCKET_IN_BUFFER:
		case SOCKET_OUT_BUFFER:
		{
			VERIFI(0, MAX_SOCKET_BUFFER_SIZE);
			break;
		}
#endif
		case NUMBER_OF_SEGMENTS:
		{
			VERIFI(1, 200);
			break;
		}
		case AUTO_SEARCH_TIME:
		{
			VER_MIN(1);
			break;
		}
		case MINIMUM_SEARCH_INTERVAL:
		{
			VER_MIN(2);
			break;
		}
		case MAX_FINISHED_UPLOADS:
		case MAX_FINISHED_DOWNLOADS:
		{
			VER_MIN(0);
			break;
		}
		case COLOR_AVOIDING:
		{
			if (value + get(PROGRESS_BACK_COLOR, false) > RGB(255, 255, 255))
			{
				value = RGB(100, 0, 0);
				l_auto = true;
			}
			break;
		}
		case PSR_DELAY:
		{
			VER_MIN(5);
			break;
		}
		case POPUP_TIME:
		{
			VERIFI(1, 15);
			break;
		}
		case MAX_MSG_LENGTH:
		{
			VERIFI(1, 512);
			break;
		}
		case POPUP_W:
		{
			VERIFI(80, 599);
			break;
		}
		case POPUP_H:
		{
			VERIFI(50, 299);
			break;
		}
		case POPUP_TRANSP:
		{
			VERIFI(50, 255);
			break;
		}
		case MAX_RESIZE_LINES:
		{
			VER_MIN(1);
			break;
		}
		case MAX_UPLOAD_SPEED_LIMIT_NORMAL:
		case MAX_DOWNLOAD_SPEED_LIMIT_NORMAL:
		case MAX_UPLOAD_SPEED_LIMIT_TIME:
		case MAX_DOWNLOAD_SPEED_LIMIT_TIME:
		case THROTTLE_ENABLE:
		{
#ifdef IRAINMAN_SPEED_LIMITER_5S4_10
#define MIN_UPLOAD_SPEED_LIMIT  5 * UploadManager::getInstance()->getSlots() + 4
#define MAX_LIMIT_RATIO         10
			if ((key == MAX_UPLOAD_SPEED_LIMIT_NORMAL || key == MAX_UPLOAD_SPEED_LIMIT_TIME) && value > 0 && value < MIN_UPLOAD_SPEED_LIMIT)
			{
				value = MIN_UPLOAD_SPEED_LIMIT;
				l_auto = true;
			}
			else if (key == MAX_DOWNLOAD_SPEED_LIMIT_NORMAL && (value == 0 || value > MAX_LIMIT_RATIO * get(MAX_UPLOAD_SPEED_LIMIT_NORMAL, false)))
			{
				value = MAX_LIMIT_RATIO * get(MAX_UPLOAD_SPEED_LIMIT_NORMAL, false);
				l_auto = true;
			}
			else if (key == MAX_DOWNLOAD_SPEED_LIMIT_TIME && (value == 0 || value > MAX_LIMIT_RATIO * get(MAX_UPLOAD_SPEED_LIMIT_TIME, false)))
			{
				value = MAX_LIMIT_RATIO * get(MAX_UPLOAD_SPEED_LIMIT_TIME, false);
				l_auto = true;
			}
#undef MIN_UPLOAD_SPEED_LIMIT
#undef MAX_LIMIT_RATIO
#endif // IRAINMAN_SPEED_LIMITER_5S4_10
			ThrottleManager::getInstance()->updateLimits(); // [+] IRainman SpeedLimiter
			break;
		}
		case IPUPDATE_INTERVAL:
		{
			VERIFI(0, 86400);
			break;
		}
		case WEBSERVER_SEARCHSIZE:
		{
			VERIFI(1, 1000);
			break;
		}
		case WEBSERVER_SEARCHPAGESIZE:
		{
			VERIFI(1, 50);
			break;
		}
		case MEDIA_PLAYER:
		{
			VERIFI(WinAmp, PlayersCount);
			break;
		}
		case TCP_PORT:
		{
			VERIFI(1, 65535);
			GET_NEW_PORT_VALUE_IF_CONFLICTS(TLS_PORT);
			GET_NEW_PORT_VALUE_IF_CONFLICTS(WEBSERVER_PORT);
			break;
		}
		case TLS_PORT:
		{
			VERIFI(1, 65535);
			GET_NEW_PORT_VALUE_IF_CONFLICTS(WEBSERVER_PORT);
			GET_NEW_PORT_VALUE_IF_CONFLICTS(TCP_PORT);
			break;
		}
		case WEBSERVER_PORT:
		{
			VERIFI(1, 65535);
			GET_NEW_PORT_VALUE_IF_CONFLICTS(TLS_PORT);
			GET_NEW_PORT_VALUE_IF_CONFLICTS(TCP_PORT);
			break;
		}
		case UDP_PORT:
		{
			VERIFI(1, 65535);
#ifdef STRONG_USE_DHT
			GET_NEW_PORT_VALUE_IF_CONFLICTS(DHT_PORT);
#endif
			break;
		}
#ifdef STRONG_USE_DHT
		case DHT_PORT:
		{
			VERIFI(1, 65535);
			GET_NEW_PORT_VALUE_IF_CONFLICTS(UDP_PORT);
			break;
		}
#endif
		case AUTOUPDATE_TIME:
		{
#ifdef HOURLY_CHECK_UPDATE
			VERIFI(0, 24);
#else
			VERIFI(0, 23);
#endif
			break;
		}
		case BANDWIDTH_LIMIT_START:
		case BANDWIDTH_LIMIT_END:
		{
			VERIFI(0, 23);
			break;
		}
		case DIRECTORYLISTINGFRAME_SPLIT:
		case UPLOADQUEUEFRAME_SPLIT:
		case QUEUEFRAME_SPLIT:
		{
			VERIFI(80, 8000);
			break;
		}
		
#undef VER_MIN
#undef VER_MAX
#undef VERIFI
#undef GET_NEW_PORT_VALUE_IF_CONFLICTS
	}
	intSettings[key - INT_FIRST] = value;
	isSet[key] = true;
	return l_auto; // [+] IRainman
}

void SettingsManager::save(const string& aFileName)
{
	SimpleXML xml;
	xml.addTag("DCPlusPlus");
	xml.stepIn();
	xml.addTag("Settings");
	xml.stepIn();
	
	int i;
	string type("type"), curType("string");
	
	for (i = STR_FIRST; i < STR_LAST; i++)
	{
		if (i == CONFIG_VERSION)
		{
			xml.addTag(settingTags[i], REVISION_NUM);
			xml.addChildAttrib(type, curType);
		}
		else if (isSet[i])
		{
			xml.addTag(settingTags[i], get(StrSetting(i), false));
			xml.addChildAttrib(type, curType);
		}
	}
	
	curType = "int";
	for (i = INT_FIRST; i < INT_LAST; i++)
	{
		if (isSet[i])
		{
			xml.addTag(settingTags[i], get(IntSetting(i), false));
			xml.addChildAttrib(type, curType);
		}
	}
	// [!] IRainman don't uncoment this code, FlylinkDC++ save all statistics on database!
	//curType = "int64";
	//for(i=INT64_FIRST; i<INT64_LAST; ++i)
	//{
	//  if(isSet[i])
	//  {
	//      xml.addTag(settingTags[i], get(Int64Setting(i), false));
	//      xml.addChildAttrib(type, curType);
	//  }
	//}
	xml.stepOut();
	
	/*xml.addTag("SearchTypes");
	xml.stepIn();
	for (auto i = g_searchTypes.cbegin(); i != g_searchTypes.cend(); ++i)
	{
	    xml.addTag("SearchType", Util::toString(";", i->second));
	    xml.addChildAttrib("Id", i->first);
	}
	xml.stepOut();*/
	
	fire(SettingsManagerListener::Save(), xml);
	
	try
	{
		File out(aFileName + ".tmp", File::WRITE, File::CREATE | File::TRUNCATE);
		BufferedOutputStream<false> f(&out, 1024);
		f.write(SimpleXML::utf8Header);
		xml.toXML(&f);
		f.flush();
		out.close();
		File::deleteFile(aFileName);
		File::renameFile(aFileName + ".tmp", aFileName);
	}
	catch (const FileException&)
	{
		// ...
	}
}

void SettingsManager::validateSearchTypeName(const string& name)
{
	if (name.empty() || (name.size() == 1 && name[0] >= '1' && name[0] <= '6'))
	{
		throw SearchTypeException("Invalid search type name"); // TODO translate
	}
	for (int type = Search::TYPE_ANY; type != Search::TYPE_LAST; ++type)
	{
		if (SearchManager::getTypeStr(type) == name)
		{
			throw SearchTypeException("This search type already exists"); // TODO translate
		}
	}
}

void SettingsManager::setSearchTypeDefaults()
{
	g_searchTypes.clear();
	
	// for conveniency, the default search exts will be the same as the ones defined by SEGA.
	const auto& searchExts = AdcHub::getSearchExts();
	for (size_t i = 0, n = searchExts.size(); i < n; ++i)
		g_searchTypes[string(1, '1' + i)] = searchExts[i];
		
	fire(SettingsManagerListener::SearchTypesChanged());
}

void SettingsManager::addSearchType(const string& name, const StringList& extensions, bool validated)
{
	if (!validated)
	{
		validateSearchTypeName(name);
	}
	
	if (g_searchTypes.find(name) != g_searchTypes.end())
	{
		throw SearchTypeException("This search type already exists"); // TODO translate
	}
	
	g_searchTypes[name] = extensions;
	fire(SettingsManagerListener::SearchTypesChanged());
}

void SettingsManager::delSearchType(const string& name)
{
	validateSearchTypeName(name);
	g_searchTypes.erase(name);
	fire(SettingsManagerListener::SearchTypesChanged());
}

void SettingsManager::renameSearchType(const string& oldName, const string& newName)
{
	validateSearchTypeName(newName);
	StringList exts = getSearchType(oldName)->second;
	addSearchType(newName, exts, TRUE);
	g_searchTypes.erase(oldName);
}

void SettingsManager::modSearchType(const string& name, const StringList& extensions)
{
	getSearchType(name)->second = extensions;
	fire(SettingsManagerListener::SearchTypesChanged());
}

const StringList& SettingsManager::getExtensions(const string& name)
{
	return getSearchType(name)->second;
}

SettingsManager::SearchTypesIter SettingsManager::getSearchType(const string& name)
{
	SearchTypesIter ret = g_searchTypes.find(name);
	if (ret == g_searchTypes.end())
	{
		throw SearchTypeException("No such search type"); // TODO translate
	}
	return ret;
}

// [+] IRainman fix: avoid to import-export color scheme in kernel.
void SettingsManager::importDctheme(const tstring& file, const bool asDefault /*= *false*/)
{

#define importData(x, y)\
	if(xml.findChild(x)){if(asDefault){setDefault(y, xml.getChildData());}else{set(y, xml.getChildData());}}\
	xml.resetCurrentChild();

	try
	{
		SimpleXML xml;
		xml.fromXML(File(file, File::READ, File::OPEN).read());
		xml.resetCurrentChild();
		xml.stepIn();
		if (xml.findChild(("Settings")))
		{
			xml.stepIn();
			importData("Font", TEXT_FONT);
			importData("BackgroundColor", BACKGROUND_COLOR);
			importData("TextColor", TEXT_COLOR);
			importData("DownloadBarColor", DOWNLOAD_BAR_COLOR);
			importData("UploadBarColor", UPLOAD_BAR_COLOR);
			importData("TextGeneralBackColor", TEXT_GENERAL_BACK_COLOR);
			importData("TextGeneralForeColor", TEXT_GENERAL_FORE_COLOR);
			importData("TextGeneralBold", TEXT_GENERAL_BOLD);
			importData("TextGeneralItalic", TEXT_GENERAL_ITALIC);
			importData("TextMyOwnBackColor", TEXT_MYOWN_BACK_COLOR);
			importData("TextMyOwnForeColor", TEXT_MYOWN_FORE_COLOR);
			importData("TextMyOwnBold", TEXT_MYOWN_BOLD);
			importData("TextMyOwnItalic", TEXT_MYOWN_ITALIC);
			importData("TextPrivateBackColor", TEXT_PRIVATE_BACK_COLOR);
			importData("TextPrivateForeColor", TEXT_PRIVATE_FORE_COLOR);
			importData("TextPrivateBold", TEXT_PRIVATE_BOLD);
			importData("TextPrivateItalic", TEXT_PRIVATE_ITALIC);
			importData("TextSystemBackColor", TEXT_SYSTEM_BACK_COLOR);
			importData("TextSystemForeColor", TEXT_SYSTEM_FORE_COLOR);
			importData("TextSystemBold", TEXT_SYSTEM_BOLD);
			importData("TextSystemItalic", TEXT_SYSTEM_ITALIC);
			importData("TextServerBackColor", TEXT_SERVER_BACK_COLOR);
			importData("TextServerForeColor", TEXT_SERVER_FORE_COLOR);
			importData("TextServerBold", TEXT_SERVER_BOLD);
			importData("TextServerItalic", TEXT_SERVER_ITALIC);
			importData("TextTimestampBackColor", TEXT_TIMESTAMP_BACK_COLOR);
			importData("TextTimestampForeColor", TEXT_TIMESTAMP_FORE_COLOR);
			importData("TextTimestampBold", TEXT_TIMESTAMP_BOLD);
			importData("TextTimestampItalic", TEXT_TIMESTAMP_ITALIC);
			importData("TextMyNickBackColor", TEXT_MYNICK_BACK_COLOR);
			importData("TextMyNickForeColor", TEXT_MYNICK_FORE_COLOR);
			importData("TextMyNickBold", TEXT_MYNICK_BOLD);
			importData("TextMyNickItalic", TEXT_MYNICK_ITALIC);
			importData("TextFavBackColor", TEXT_FAV_BACK_COLOR);
			importData("TextFavForeColor", TEXT_FAV_FORE_COLOR);
			importData("TextFavBold", TEXT_FAV_BOLD);
			importData("TextFavItalic", TEXT_FAV_ITALIC);
			importData("TextURLBackColor", TEXT_URL_BACK_COLOR);
			importData("TextURLForeColor", TEXT_URL_FORE_COLOR);
			importData("TextURLBold", TEXT_URL_BOLD);
			importData("TextURLItalic", TEXT_URL_ITALIC);
			importData("BoldAuthorsMess", BOLD_AUTHOR_MESS);
			importData("ProgressTextDown", PROGRESS_TEXT_COLOR_DOWN);
			importData("ProgressTextUp", PROGRESS_TEXT_COLOR_UP);
			importData("ErrorColor", ERROR_COLOR);
			importData("ProgressOverrideColors", PROGRESS_OVERRIDE_COLORS);
			importData("MenubarTwoColors", MENUBAR_TWO_COLORS);
			importData("MenubarLeftColor", MENUBAR_LEFT_COLOR);
			importData("MenubarRightColor", MENUBAR_RIGHT_COLOR);
			importData("MenubarBumped", MENUBAR_BUMPED);
			importData("Progress3DDepth", PROGRESS_3DDEPTH);
			importData("ProgressOverrideColors2", PROGRESS_OVERRIDE_COLORS2);
			importData("TextOPBackColor", TEXT_OP_BACK_COLOR);
			importData("TextOPForeColor", TEXT_OP_FORE_COLOR);
			importData("TextOPBold", TEXT_OP_BOLD);
			importData("TextOPItalic", TEXT_OP_ITALIC);
			importData("TextEnemyBackColor", TEXT_ENEMY_BACK_COLOR);
			importData("TextEnemyForeColor", TEXT_ENEMY_FORE_COLOR);
			importData("TextEnemyBold", TEXT_ENEMY_BOLD);
			importData("TextEnemyItalic", TEXT_ENEMY_ITALIC);
			importData("SearchAlternateColour", SEARCH_ALTERNATE_COLOUR);
			importData("ProgressBackColor", PROGRESS_BACK_COLOR);
			importData("ProgressCompressColor", PROGRESS_COMPRESS_COLOR);
			importData("ProgressSegmentColor", PROGRESS_SEGMENT_COLOR);
			importData("ColorDownloaded", COLOR_DOWNLOADED);
			importData("ColorRunning", COLOR_RUNNING);
			importData("ColorVerified", COLOR_VERIFIED);
			importData("ColorAvoiding", COLOR_AVOIDING);
			importData("ReservedSlotColor", RESERVED_SLOT_COLOR);
			importData("IgnoredColor", IGNORED_COLOR);
			importData("FavoriteColor", FAVORITE_COLOR);
			importData("NormalColour", NORMAL_COLOUR);
			importData("FireballColor", FIREBALL_COLOR);
			importData("ServerColor", SERVER_COLOR);
			importData("PasiveColor", PASIVE_COLOR);
			importData("OpColor", OP_COLOR);
			importData("FileListAndClientCheckedColour", FULL_CHECKED_COLOUR);
			importData("BadClientColour", BAD_CLIENT_COLOUR);
			importData("BadFilelistColour", BAD_FILELIST_COLOUR);
			importData("ProgressbaroDCStyle", PROGRESSBAR_ODC_STYLE);
			importData("UseCustomListBackground", USE_CUSTOM_LIST_BACKGROUND);
			// Tab Colors
			importData("TabSelectedColor", TAB_SELECTED_COLOR);
			importData("TabOfflineColor", TAB_OFFLINE_COLOR);
			importData("TabActivityColor", TAB_ACTIVITY_COLOR);
			importData("TabSelectedTextColor", TAB_SELECTED_TEXT_COLOR);
			importData("TabOfflineTextColor", TAB_OFFLINE_TEXT_COLOR);
			importData("TabActivityTextColor", TAB_ACTIVITY_TEXT_COLOR);
			// Favorite Hubs Colors
#ifdef SCALOLAZ_USE_COLOR_HUB_IN_FAV
			importData("HubInFavoriteBkColor", HUB_IN_FAV_BK_COLOR);
			importData("HubInFavoriteConnectBkColor", HUB_IN_FAV_CONNECT_BK_COLOR);
#endif
			// FileList Colors
			importData("BanColor", BAN_COLOR);
			importData("DupeColor", DUPE_COLOR);
			importData("DupeEx1Color", DUPE_EX1_COLOR);
			importData("DupeEx2Color", DUPE_EX2_COLOR);
			// Popup Colors
			importData("PopupMaxMsgLen", MAX_MSG_LENGTH);
			importData("PopupFoneImage", POPUP_IMAGE);
			importData("PopupFoneImageFile", POPUPFILE);
			importData("PopupTypeBalloon", POPUP_TYPE);
			importData("PopupTime", POPUP_TIME);
			importData("PopupFont", POPUP_FONT);
			importData("PopupTitleFont", POPUP_TITLE_FONT);
			importData("PopupBackColor", POPUP_BACKCOLOR);
			importData("PopupTextColor", POPUP_TEXTCOLOR);
			importData("PopupTitleTextColor", POPUP_TITLE_TEXTCOLOR);
		}
		xml.resetCurrentChild();
		xml.stepOut();
	}
	catch (const FileException& e)
	{
		LogManager::getInstance()->message(STRING(COULD_NOT_OPEN_TARGET_FILE) + e.getError());
	}
	catch (const SimpleXMLException& e)
	{
		LogManager::getInstance()->message(STRING(COULD_NOT_PARSE_XML_DATA) + e.getError());
	}
	
#undef importData
}

void SettingsManager::exportDctheme(const tstring& file)
{

#define exportData(x, y)\
	xml.addTag(x, SETTING(y));\
	xml.addChildAttrib(type, typeid(y) == typeid(StrSetting) ? stringType : intType);

	static const string type("type");
	static const string stringType("string");
	static const string intType("int");
	
	SimpleXML xml;
	xml.addTag("DCPlusPlus");
	xml.stepIn();
	xml.addTag("Settings");
	xml.stepIn();
	
	exportData("Font", TEXT_FONT);
	exportData("BackgroundColor", BACKGROUND_COLOR);
	exportData("TextColor", TEXT_COLOR);
	exportData("DownloadBarColor", DOWNLOAD_BAR_COLOR);
	exportData("UploadBarColor", UPLOAD_BAR_COLOR);
	exportData("TextGeneralBackColor", TEXT_GENERAL_BACK_COLOR);
	exportData("TextGeneralForeColor", TEXT_GENERAL_FORE_COLOR);
	exportData("TextGeneralBold", TEXT_GENERAL_BOLD);
	exportData("TextGeneralItalic", TEXT_GENERAL_ITALIC);
	exportData("TextMyOwnBackColor", TEXT_MYOWN_BACK_COLOR);
	exportData("TextMyOwnForeColor", TEXT_MYOWN_FORE_COLOR);
	exportData("TextMyOwnBold", TEXT_MYOWN_BOLD);
	exportData("TextMyOwnItalic", TEXT_MYOWN_ITALIC);
	exportData("TextPrivateBackColor", TEXT_PRIVATE_BACK_COLOR);
	exportData("TextPrivateForeColor", TEXT_PRIVATE_FORE_COLOR);
	exportData("TextPrivateBold", TEXT_PRIVATE_BOLD);
	exportData("TextPrivateItalic", TEXT_PRIVATE_ITALIC);
	exportData("TextSystemBackColor", TEXT_SYSTEM_BACK_COLOR);
	exportData("TextSystemForeColor", TEXT_SYSTEM_FORE_COLOR);
	exportData("TextSystemBold", TEXT_SYSTEM_BOLD);
	exportData("TextSystemItalic", TEXT_SYSTEM_ITALIC);
	exportData("TextServerBackColor", TEXT_SERVER_BACK_COLOR);
	exportData("TextServerForeColor", TEXT_SERVER_FORE_COLOR);
	exportData("TextServerBold", TEXT_SERVER_BOLD);
	exportData("TextServerItalic", TEXT_SERVER_ITALIC);
	exportData("TextTimestampBackColor", TEXT_TIMESTAMP_BACK_COLOR);
	exportData("TextTimestampForeColor", TEXT_TIMESTAMP_FORE_COLOR);
	exportData("TextTimestampBold", TEXT_TIMESTAMP_BOLD);
	exportData("TextTimestampItalic", TEXT_TIMESTAMP_ITALIC);
	exportData("TextMyNickBackColor", TEXT_MYNICK_BACK_COLOR);
	exportData("TextMyNickForeColor", TEXT_MYNICK_FORE_COLOR);
	exportData("TextMyNickBold", TEXT_MYNICK_BOLD);
	exportData("TextMyNickItalic", TEXT_MYNICK_ITALIC);
	exportData("TextFavBackColor", TEXT_FAV_BACK_COLOR);
	exportData("TextFavForeColor", TEXT_FAV_FORE_COLOR);
	exportData("TextFavBold", TEXT_FAV_BOLD);
	exportData("TextFavItalic", TEXT_FAV_ITALIC);
	exportData("TextURLBackColor", TEXT_URL_BACK_COLOR);
	exportData("TextURLForeColor", TEXT_URL_FORE_COLOR);
	exportData("TextURLBold", TEXT_URL_BOLD);
	exportData("TextURLItalic", TEXT_URL_ITALIC);
	exportData("BoldAuthorsMess", BOLD_AUTHOR_MESS);
	exportData("ProgressTextDown", PROGRESS_TEXT_COLOR_DOWN);
	exportData("ProgressTextUp", PROGRESS_TEXT_COLOR_UP);
	exportData("ErrorColor", ERROR_COLOR);
	exportData("ProgressOverrideColors", PROGRESS_OVERRIDE_COLORS);
	exportData("MenubarTwoColors", MENUBAR_TWO_COLORS);
	exportData("MenubarLeftColor", MENUBAR_LEFT_COLOR);
	exportData("MenubarRightColor", MENUBAR_RIGHT_COLOR);
	exportData("MenubarBumped", MENUBAR_BUMPED);
	exportData("Progress3DDepth", PROGRESS_3DDEPTH);
	exportData("ProgressOverrideColors2", PROGRESS_OVERRIDE_COLORS2);
	exportData("TextOPBackColor", TEXT_OP_BACK_COLOR);
	exportData("TextOPForeColor", TEXT_OP_FORE_COLOR);
	exportData("TextOPBold", TEXT_OP_BOLD);
	exportData("TextOPItalic", TEXT_OP_ITALIC);
	exportData("TextEnemyBackColor", TEXT_ENEMY_BACK_COLOR);
	exportData("TextEnemyForeColor", TEXT_ENEMY_FORE_COLOR);
	exportData("TextEnemyBold", TEXT_ENEMY_BOLD);
	exportData("TextEnemyItalic", TEXT_ENEMY_ITALIC);
	exportData("SearchAlternateColour", SEARCH_ALTERNATE_COLOUR);
	exportData("ProgressBackColor", PROGRESS_BACK_COLOR);
	exportData("ProgressCompressColor", PROGRESS_COMPRESS_COLOR);
	exportData("ProgressSegmentColor", PROGRESS_SEGMENT_COLOR);
	exportData("ColorDownloaded", COLOR_DOWNLOADED);
	exportData("ColorRunning", COLOR_RUNNING);
	exportData("ColorVerified", COLOR_VERIFIED);
	exportData("ColorAvoiding", COLOR_AVOIDING);
	exportData("ReservedSlotColor", RESERVED_SLOT_COLOR);
	exportData("IgnoredColor", IGNORED_COLOR);
	exportData("FavoriteColor", FAVORITE_COLOR);
	exportData("NormalColour", NORMAL_COLOUR);
	exportData("FireballColor", FIREBALL_COLOR);
	exportData("ServerColor", SERVER_COLOR);
	exportData("PasiveColor", PASIVE_COLOR);
	exportData("OpColor", OP_COLOR);
	exportData("FileListAndClientCheckedColour", FULL_CHECKED_COLOUR);
	exportData("BadClientColour", BAD_CLIENT_COLOUR);
	exportData("BadFilelistColour", BAD_FILELIST_COLOUR);
	exportData("ProgressbaroDCStyle", PROGRESSBAR_ODC_STYLE);
	exportData("UseCustomListBackground", USE_CUSTOM_LIST_BACKGROUND);
	// Tab Colors
	exportData("TabSelectedColor", TAB_SELECTED_COLOR);
	exportData("TabOfflineColor", TAB_OFFLINE_COLOR);
	exportData("TabActivityColor", TAB_ACTIVITY_COLOR);
	exportData("TabSelectedTextColor", TAB_SELECTED_TEXT_COLOR);
	exportData("TabOfflineTextColor", TAB_OFFLINE_TEXT_COLOR);
	exportData("TabActivityTextColor", TAB_ACTIVITY_TEXT_COLOR);
	// Favorite Hubs Colors
#ifdef SCALOLAZ_USE_COLOR_HUB_IN_FAV
	exportData("HubInFavoriteBkColor", HUB_IN_FAV_BK_COLOR);
	exportData("HubInFavoriteConnectBkColor", HUB_IN_FAV_CONNECT_BK_COLOR);
#endif
	// FileList Colors
	exportData("BanColor", BAN_COLOR);
	exportData("DupeColor", DUPE_COLOR);
	exportData("DupeEx1Color", DUPE_EX1_COLOR);
	exportData("DupeEx2Color", DUPE_EX2_COLOR);
	// Popup Colors
	exportData("PopupMaxMsgLen", MAX_MSG_LENGTH);
	exportData("PopupFoneImage", POPUP_IMAGE);
	exportData("PopupFoneImageFile", POPUPFILE);
	exportData("PopupTypeBalloon", POPUP_TYPE);
	exportData("PopupTime", POPUP_TIME);
	exportData("PopupFont", POPUP_FONT);
	exportData("PopupTitleFont", POPUP_TITLE_FONT);
	exportData("PopupBackColor", POPUP_BACKCOLOR);
	exportData("PopupTextColor", POPUP_TEXTCOLOR);
	exportData("PopupTitleTextColor", POPUP_TITLE_TEXTCOLOR);
	
	try
	{
		File l_ff(file , File::WRITE, File::CREATE | File::TRUNCATE);
		BufferedOutputStream<false> f(&l_ff, 1024);
		f.write(SimpleXML::utf8Header);
		xml.toXML(&f);
		f.flush();
		l_ff.close();
	}
	catch (const FileException& e)
	{
		LogManager::getInstance()->message(STRING(COULD_NOT_OPEN_TARGET_FILE) + e.getError());
	}
	
#undef exportData
}
// [~] IRainman fix: avoid to import-export color scheme in kernel.

/**
 * @file
 * $Id: SettingsManager.cpp 575 2011-08-25 19:38:04Z bigmuscle $
 */
