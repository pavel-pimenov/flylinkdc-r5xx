#pragma once
#ifndef FLYSQLEXPLORER_A95CE591_BB0C_46C4_A4C4_14A02B939434
#define FLYSQLEXPLORER_A95CE591_BB0C_46C4_A4C4_14A02B939434

#ifdef FLYLINKDC_USE_SQL_EXPLORER

#include "FlatTabCtrl.h"
#include "TypedListViewCtrl.h"
#include "UCHandler.h"

#include "../client/DirectoryListing.h"
#include "../client/StringSearch.h"
#include "../client/ADLSearch.h"
#include "../client/ShareManager.h"
#include "../FlyFeatures/VideoPreview.h"
#include "../client/SettingsManager.h"

namespace FlyWindow
{

class ItemInfo;

class FlySQLExplorer : boost::noncopyable
	, public MDITabChildWindowImpl < FlySQLExplorer, RGB(255, 0, 255), IDR_FILE_LIST>
	, public CSplitterImpl<FlySQLExplorer>
	, public UCHandler<FlySQLExplorer>
	, private SettingsManagerListener
{
	private:
		FlySQLExplorer();
		~FlySQLExplorer();
	private:
		enum
		{
			FINISHED,
			ABORTED
		};
		
	public:
		DECLARE_FRAME_WND_CLASS(_T("FlySQLExplorer"), IDR_FILE_LIST)
	private:
		typedef MDITabChildWindowImpl < FlySQLExplorer, RGB(255, 0, 255), IDR_FILE_LIST > baseClass;
		typedef UCHandler<FlySQLExplorer>       ucBase;
		typedef std::map < HWND, FlySQLExplorer* >  FrameMap;
		typedef std::pair < HWND, FlySQLExplorer* > FramePair;
		
	public:
		// Таблица откликов
		BEGIN_MSG_MAP(FlySQLExplorer)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		CHAIN_COMMANDS(ucBase)
		CHAIN_MSG_MAP(baseClass)
		CHAIN_MSG_MAP(CSplitterImpl<FlySQLExplorer>)
		ALT_MSG_MAP(m_statusMessageMap)
		ALT_MSG_MAP(m_controlMessageMap)
		END_MSG_MAP()
		
	public:
		// Обработчики таблицы откликов
		LRESULT OnCreate(UINT, WPARAM, LPARAM, BOOL& bHandled);
		LRESULT onClose(UINT, WPARAM, LPARAM, BOOL& bHandled);
		LRESULT onSpeaker(UINT, WPARAM wParam, LPARAM, BOOL& bHandled);
		static FlySQLExplorer *Instance();
		
		void UpdateLayout(BOOL bResizeBars = TRUE);
		void runUserCommand(UserCommand& uc);
		
	private:
		void setWindowTitle();
		
		CContainedWindow        m_statusContainer;
		CContainedWindow        m_treeContainer;
		CContainedWindow        m_listContainer;
		CTreeViewCtrl           m_ctrlTree;
		TypedListViewCtrl<ItemInfo, IDC_FILES>  m_ctrlList;
		CStatusBarCtrl                          m_ctrlStatus;
		HTREEITEM                               m_treeRoot;
		int const static                        m_statusMessageMap = 20;
		int const static                        m_controlMessageMap = 21;
		static FlySQLExplorer                   *m_flySQLExplorer;
};

class ItemInfo
{
		friend FlySQLExplorer;
		
	public:
		ItemInfo();
		~ItemInfo();
	public:
		int getImageIndex() const;
};

}

#endif // FLYLINKDC_USE_SQL_EXPLORER

#endif // FLYSQLEXPLORER_A95CE591_BB0C_46C4_A4C4_14A02B939434