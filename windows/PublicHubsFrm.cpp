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

#include "stdafx.h"
#include "Resource.h"
#include "PublicHubsFrm.h"
#include "HubFrame.h"
#include "WinUtil.h"
#include "../client/BZUtils.h"
#include <boost/algorithm/string.hpp>
#include "../FlyFeatures/flyServer.h"

int PublicHubsFrame::columnIndexes[] =
{
	COLUMN_NAME,
	COLUMN_DESCRIPTION,
	COLUMN_USERS,
	COLUMN_SERVER,
	COLUMN_COUNTRY,
	COLUMN_SHARED,
	COLUMN_MINSHARE,
	COLUMN_MINSLOTS,
	COLUMN_MAXHUBS,
	COLUMN_MAXUSERS,
	COLUMN_RELIABILITY,
	COLUMN_RATING
};

int PublicHubsFrame::columnSizes[] = { 200, 290, 50, 100, 100, 100, 100, 100, 100, 100, 100, 100 };

static ResourceManager::Strings columnNames[] =
{
	ResourceManager::HUB_NAME,
	ResourceManager::DESCRIPTION,
	ResourceManager::USERS,
	ResourceManager::HUB_ADDRESS,
	ResourceManager::COUNTRY,
	ResourceManager::SHARED,
	ResourceManager::MIN_SHARE,
	ResourceManager::MIN_SLOTS,
	ResourceManager::MAX_HUBS,
	ResourceManager::MAX_USERS,
	ResourceManager::RELIABILITY,
	ResourceManager::RATING,
};

LRESULT PublicHubsFrame::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);
	ctrlStatus.Attach(m_hWndStatusBar);
	
	int w[3] = { 0, 0, 0 };
	ctrlStatus.SetParts(3, w);
	
	m_ctrlHubs.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                  WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS |
	                  LVS_SHAREIMAGELISTS, // https://github.com/pavel-pimenov/flylinkdc-r5xx/issues/1611
	                  WS_EX_CLIENTEDGE, IDC_HUBLIST);
	SET_EXTENDENT_LIST_VIEW_STYLE(m_ctrlHubs);
	
	// Create listview columns
	WinUtil::splitTokens(columnIndexes, SETTING(PUBLICHUBSFRAME_ORDER), COLUMN_LAST);
	WinUtil::splitTokensWidth(columnSizes, SETTING(PUBLICHUBSFRAME_WIDTHS), COLUMN_LAST);
	
	BOOST_STATIC_ASSERT(_countof(columnSizes) == COLUMN_LAST);
	BOOST_STATIC_ASSERT(_countof(columnNames) == COLUMN_LAST);
	for (int j = 0; j < COLUMN_LAST; j++)
	{
		const int fmt = (j == COLUMN_USERS) ? LVCFMT_RIGHT : LVCFMT_LEFT;
		m_ctrlHubs.InsertColumn(j, CTSTRING_I(columnNames[j]), fmt, columnSizes[j], j);
	}
	
	m_ctrlHubs.SetColumnOrderArray(COLUMN_LAST, columnIndexes);
	
	SET_LIST_COLOR(m_ctrlHubs);
	
	m_ctrlHubs.SetImageList(g_flagImage.getIconList(), LVSIL_SMALL);
	
	/*  extern HIconWrapper g_hOfflineIco;
	  extern HIconWrapper g_hOnlineIco;
	    m_onlineStatusImg.Create(16, 16, ILC_COLOR32 | ILC_MASK,  0, 2);
	    m_onlineStatusImg.AddIcon(g_hOnlineIco);
	    m_onlineStatusImg.AddIcon(g_hOfflineIco);
	  m_ctrlHubs.SetImageList(m_onlineStatusImg, LVSIL_SMALL);
	*/
	ClientManager::getOnlineClients(m_onlineHubs);
	
	m_ctrlHubs.SetFocus();
	
	m_ctrlTree.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_HASLINES | TVS_SHOWSELALWAYS | TVS_DISABLEDRAGDROP, WS_EX_CLIENTEDGE, IDC_ISP_TREE);
	m_ctrlTree.SetBkColor(Colors::g_bgColor);
	m_ctrlTree.SetTextColor(Colors::g_textColor);
	WinUtil::SetWindowThemeExplorer(m_ctrlTree.m_hWnd);
	
	m_treeContainer.SubclassWindow(m_ctrlTree);
	
	
	SetSplitterExtendedStyle(SPLIT_PROPORTIONAL); // При изменении размеров окна-контейнера размеры разделяемых областей меняются пропорционально.
	SetSplitterPanes(m_ctrlTree.m_hWnd, m_ctrlHubs.m_hWnd);
	m_nProportionalPos = SETTING(FLYSERVER_HUBLIST_SPLIT);
	
	ctrlFilter.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                  ES_AUTOHSCROLL, WS_EX_CLIENTEDGE);
	m_filterContainer.SubclassWindow(ctrlFilter.m_hWnd);
	ctrlFilter.SetFont(Fonts::g_systemFont);
	
	ctrlFilterSel.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                     WS_HSCROLL | WS_VSCROLL | CBS_DROPDOWNLIST, WS_EX_CLIENTEDGE);
	ctrlFilterSel.SetFont(Fonts::g_systemFont, FALSE);
	
	//populate the filter list with the column names
	for (int j = 0; j < COLUMN_LAST; j++)
	{
		ctrlFilterSel.AddString(CTSTRING_I(columnNames[j]));
	}
	ctrlFilterSel.AddString(CTSTRING(ANY));
	ctrlFilterSel.SetCurSel(COLUMN_LAST);
	
	ctrlFilterDesc.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                      BS_GROUPBOX, WS_EX_TRANSPARENT);
	ctrlFilterDesc.SetWindowText(CTSTRING(FILTER));
	ctrlFilterDesc.SetFont(Fonts::g_systemFont);
	
	SettingsManager::getInstance()->addListener(this);
	
	updateList();
	loadISPHubs();
	m_ctrlHubs.setSort(COLUMN_USERS, ExListViewCtrl::SORT_INT, false);
	/*  const int l_sort = SETTING(HUBS_PUBLIC_COLUMNS_SORT);
	    int l_sort_type = ExListViewCtrl::SORT_STRING_NOCASE;
	    if (l_sort == 2 || l_sort > 4)
	        l_sort_type = ExListViewCtrl::SORT_INT;
	    if (l_sort == 5)
	        l_sort_type = ExListViewCtrl::SORT_BYTES;
	    m_ctrlHubs.setSort(SETTING(HUBS_PUBLIC_COLUMNS_SORT), l_sort_type, BOOLSETTING(HUBS_PUBLIC_COLUMNS_SORT_ASC));
	*/
	hubsMenu.CreatePopupMenu();
	hubsMenu.AppendMenu(MF_STRING, IDC_CONNECT, CTSTRING(CONNECT));
	hubsMenu.AppendMenu(MF_STRING, IDC_ADD, CTSTRING(ADD_TO_FAVORITES_HUBS));
	hubsMenu.AppendMenu(MF_STRING, IDC_REM_AS_FAVORITE, CTSTRING(REMOVE_FROM_FAVORITES_HUBS));
	hubsMenu.AppendMenu(MF_STRING, IDC_COPY_HUB, CTSTRING(COPY_HUB));
	hubsMenu.SetMenuDefaultItem(IDC_CONNECT);
	
	bHandled = FALSE;
	return TRUE;
}

class PubHubListXmlListLoader : public SimpleXMLReader::CallBack
{
	public:
		PubHubListXmlListLoader(HubEntryList& lst) : publicHubs(lst) { }
		~PubHubListXmlListLoader() { }
		void startTag(const string& p_name, StringPairList& p_attribs, bool)
		{
			if (p_name == "Hub")
			{
				const string& l_name = getAttrib(p_attribs, "Name", 0); // Local declaration of 'name' hides declaration of the same name in outer scope.
				const string& server = getAttrib(p_attribs, "Address", 1);
				const string& description = getAttrib(p_attribs, "Description", 2);
				const string& users = getAttrib(p_attribs, "Users", 3);
				const string& country = getAttrib(p_attribs, "Country", 4); //-V112
				const string& shared = getAttrib(p_attribs, "Shared", 5);
				const string& minShare = getAttrib(p_attribs, "Minshare", 5);
				const string& minSlots = getAttrib(p_attribs, "Minslots", 5);
				const string& maxHubs = getAttrib(p_attribs, "Maxhubs", 5);
				const string& maxUsers = getAttrib(p_attribs, "Maxusers", 5);
				const string& reliability = getAttrib(p_attribs, "Reliability", 5);
				const string& rating = getAttrib(p_attribs, "Rating", 5);
				publicHubs.push_back(HubEntry(l_name, server, description, users, country, shared, minShare, minSlots, maxHubs, maxUsers, reliability, rating));
			}
		}
		void endTag(const string&, const string&)
		{
		
		}
	private:
		HubEntryList& publicHubs;
};

LRESULT PublicHubsFrame::onSelChangedISPTree(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMTREEVIEW* p = (NMTREEVIEW*) pnmh;
	if (p->itemNew.state & TVIS_SELECTED)
	{
		CWaitCursor l_cursor_wait; //-V808
		m_ctrlHubs.DeleteAllItems();
		if (p->itemNew.lParam == e_HubListItem)
		{
			tstring buf;
			buf.resize(100);
			if (m_ctrlTree.GetItemText(p->itemNew.hItem, &buf[0], buf.size()))
			{
				const string l_url = Text::fromT(buf.c_str());
				CFlyLog l_log("Download Hub List");
				std::vector<byte> l_data;
				l_log.step("URL = " + l_url);
				CFlyHTTPDownloader l_http_downloader;
				//l_http_downloader.setInetFlag(INTERNET_FLAG_RESYNCHRONIZE);
				l_http_downloader.m_is_use_cache = true;
				if (l_http_downloader.getBinaryDataFromInet(l_url, l_data, 1000))
				{
					const string ext = Util::getFileExt(l_url);
					if (stricmp(ext, ".bz2") == 0)
					{
						std::vector<byte> l_out(l_data.size() * 20);
						size_t outSize = l_out.size();
						size_t sizeRead = l_data.size();
						UnBZFilter unbzip;
						try
						{
							unbzip(l_data.data(), (size_t &)sizeRead, &l_out[0], outSize);
							l_out[outSize] = 0;
							HubEntryList& l_list = m_publicListMatrix[l_url];
							l_list.clear();
							try
							{
								PubHubListXmlListLoader loader(l_list);
								SimpleXMLReader(&loader).parse((const char*)l_out.data(), outSize, false);
								m_hubs = l_list;
								updateList();
							}
							catch (const Exception& e)
							{
								l_log.step("XML parse error = " + e.getError());
							}
						}
						catch (const Exception& e)
						{
							l_log.step("Unpack error = " + e.getError());
						}
					}
				}
			}
		}
		else
		{
			TStringSet l_items;
			LoadTreeItems(l_items, p->itemNew.hItem);
			int cnt = 0;
			for (auto i = l_items.cbegin(); i != l_items.end(); ++i)
			{
				TStringList l;
				l.resize(COLUMN_LAST);
				l[COLUMN_SERVER] = *i;
				m_ctrlHubs.insert(cnt++, l, I_IMAGECALLBACK); // !SMT!-IP
			}
		}
		m_ctrlHubs.resort();
		updateStatus();
	}
	return 0;
}

void PublicHubsFrame::loadPublicListHubs()
{
	m_PublicListRootItem = m_ctrlTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
	                                             _T("Public Hub List"),
	                                             g_ISPImage.m_flagImageCount + 2, // nImage
	                                             g_ISPImage.m_flagImageCount + 2, // nSelectedImage
	                                             0, // nState
	                                             0, // nStateMask
	                                             e_HubListRoot, // lParam
	                                             0, // aParent,
	                                             0  // hInsertAfter
	                                            );
	const StringList lists = CFlyServerConfig::getHubListUrl();
	HTREEITEM p_first_item = nullptr;
	for (auto i = lists.cbegin(); i != lists.cend(); ++i)
	{
		const auto l_item = m_ctrlTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
		                                          Text::toT(*i).c_str(),
		                                          g_ISPImage.m_flagImageCount + 15, // nImage
		                                          g_ISPImage.m_flagImageCount + 15, // nSelectedImage
		                                          0, // nState
		                                          0, // nStateMask
		                                          e_HubListItem, // lParam
		                                          m_PublicListRootItem, // aParent,
		                                          0  // hInsertAfter
		                                         );
		if (!p_first_item)
			p_first_item = l_item;
	}
	m_ctrlTree.SelectItem(p_first_item);
}

void PublicHubsFrame::loadISPHubs()
{
	CWaitCursor l_cursor_wait; //-V808
	CFlyLockWindowUpdate l_lock_update(m_ctrlTree);
	CLockRedraw<true> l_lock_draw(m_ctrlTree);
	m_ISPRootItem = 0;
	m_PublicListRootItem = 0;
	m_ctrlTree.DeleteAllItems();
	m_country_map.m_country.clear();
	g_ISPImage.init();
	m_ctrlTree.SetImageList(g_ISPImage.getIconList(), TVSIL_NORMAL);
	CFlyLog l_log("[ISP Hub Loader]");
	if (m_isp_raw_data.empty())
	{
		const string l_url_config_file = "http://etc.fly-server.ru/etc/isp-hub-list.txt";
		l_log.step("Download:" + l_url_config_file);
		if (Util::getDataFromInetSafe(true, l_url_config_file, m_isp_raw_data, 0) == 0)
		{
			l_log.step("Error download! Config will be loaded from internal resources");
			LPCSTR l_res_data;
			if (const auto l_size_res = Util::GetTextResource(IDR_ISP_HUB_LIST_EXAMPLE, l_res_data))
			{
				m_isp_raw_data = string(l_res_data, l_size_res);
			}
			else
			{
				l_log.step("Error load resource Util::GetTextResource(IDR_FLY_SERVER_CONFIG");
			}
		}
	}
	l_log.step("Parse ISP Hub list");
	if (!m_isp_raw_data.empty())
	{
		loadPublicListHubs();
		m_ISPRootItem = m_ctrlTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
		                                      _T("ISP"),
		                                      g_ISPImage.m_flagImageCount + 14, // nImage
		                                      g_ISPImage.m_flagImageCount + 14, // nSelectedImage
		                                      0, // nState
		                                      0, // nStateMask
		                                      e_ISPRoot, // lParam
		                                      0, // aParent,
		                                      0  // hInsertAfter
		                                     );
		string::size_type linestart = 0;
		string::size_type lineend = 0;
		for (;;)
		{
			lineend = m_isp_raw_data.find('\n', linestart);
			if (lineend == string::npos)
			{
				const string l_line = m_isp_raw_data.substr(linestart);
				if (!l_line.empty())
				{
					parseISPHubsLine(l_line, l_log);
				}
				break;
			}
			else
			{
				string l_line = m_isp_raw_data.substr(linestart, lineend - linestart - 1);
				parseISPHubsLine(l_line, l_log);
				linestart = lineend + 1;
			}
		}
		m_ctrlTree.Expand(m_ISPRootItem);
		m_ctrlTree.Expand(m_PublicListRootItem);
	}
}
int PublicHubsFrame::calcISPCountryIconIndex(tstring& p_country)
{
	// http://www.artlebedev.ru/tools/country-list/
	static const TCHAR* g_country_map[] =
	{
		_T("Россия"), _T("RU"),
		_T("Украина"), _T("UA"),
		_T("Беларусь"), _T("BY"),
		_T("USA"), _T("US"),
		_T("Казахстан"), _T("KZ"),
		_T("Узбекистан"), _T("UZ"),
		_T("Азербайджан"), _T("AZ"),
		_T("Молдова"), _T("MD"),
		_T("Эстония"), _T("EE"),
		_T("Bulgaria"), _T("BG"),
		_T("Brazil"), _T("BR"),
		_T("Australia"), _T("AS"),
		_T("Poland"), _T("PL"),
		_T("Korea"), _T("KP"),
		_T("Romania"), _T("RO"),
		_T("Spain"), _T("ES"),
		_T("Niue"), _T("NU"),
		_T("Italy"), _T("IT")
	};
	for (auto i = 0; i < _countof(g_country_map) / 2; ++i)
	{
		const StringTokenizer<tstring, TStringList> l_country(g_country_map[i * 2], _T(','));
		for (auto j = l_country.getTokens().cbegin(); j != l_country.getTokens().cend() ; ++j)
		{
			if (*j == p_country)
			{
				p_country = l_country.getTokens()[0];
				const auto l_id_country = Text::fromT(g_country_map[i * 2 + 1]);
				return Util::getFlagIndexByCode(*reinterpret_cast<const uint16_t*>(l_id_country.c_str()));
			}
		}
	}
	return -1;
}
void PublicHubsFrame::parseISPHubsLine(const string& p_line, CFlyLog& p_log)
{
	if (!m_filter.empty() && Text::toLower(p_line).find(Text::toLower(m_filter)) == string::npos)
		return;
	string l_line = p_line;
	const StringTokenizer<string> l_isp(l_line, ';');
	dcassert(l_isp.getTokens().size() == 6);
	if (l_isp.getTokens().size() != 6)
	{
		p_log.step("Error l_isp.getTokens().size()!= 6,  Line = " + p_line);
	}
	else
	{
		const auto l_country_name = l_isp.getTokens()[1];
		auto l_name = Text::toT(l_country_name);
		const auto l_image_index = calcISPCountryIconIndex(l_name);
		auto& l_country_item = m_country_map.m_country[l_country_name];
		
		if (!l_country_item.m_tree_item)
		{
			l_country_item.m_tree_item = m_ctrlTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
			                                                   l_name.c_str(),
			                                                   l_image_index, // nImage
			                                                   l_image_index, // nSelectedImage
			                                                   0, // nState
			                                                   0, // nStateMask
			                                                   e_ISPCountry, // lParam
			                                                   m_ISPRootItem, // aParent,
			                                                   0  // hInsertAfter
			                                                  );
		}
		// Город
		const auto l_city_name = l_isp.getTokens()[2];
		auto& l_city_item =  l_country_item.m_city[l_city_name];
		if (!l_city_item.m_tree_item)
		{
			l_city_item.m_tree_item = m_ctrlTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
			                                                Text::toT(l_city_name).c_str(),
			                                                g_ISPImage.m_flagImageCount + 7, // nImage
			                                                g_ISPImage.m_flagImageCount + 7, // nSelectedImage
			                                                0, // nState
			                                                0, // nStateMask
			                                                e_ISPCity, // lParam
			                                                l_country_item.m_tree_item, // aParent,
			                                                0  // hInsertAfter
			                                               );
		}
		// Провайдер
		const auto l_isp_name = l_isp.getTokens()[3];
		auto& l_isp_item =  l_city_item.m_isp[l_isp_name];
		if (!l_isp_item.m_tree_item)
		{
			l_isp_item.m_tree_item = m_ctrlTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
			                                               Text::toT(l_isp_name).c_str(),
			                                               g_ISPImage.m_flagImageCount + 0, // nImage
			                                               g_ISPImage.m_flagImageCount + 0, // nSelectedImage
			                                               0, // nState
			                                               0, // nStateMask
			                                               e_ISPProvider, // lParam
			                                               l_city_item.m_tree_item, // aParent,
			                                               0  // hInsertAfter
			                                              );
		}
		// Сеть
		const auto l_network_name = l_isp.getTokens()[4];
		auto& l_network_item      =  l_isp_item.m_network[l_network_name];
		if (!l_network_name.empty())
		{
			if (!l_network_item.m_tree_item)
			{
				l_network_item.m_tree_item = m_ctrlTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
				                                                   Text::toT(l_network_name).c_str(),
				                                                   g_ISPImage.m_flagImageCount + 2, // nImage
				                                                   g_ISPImage.m_flagImageCount + 2, // nSelectedImage
				                                                   0, // nState
				                                                   0, // nStateMask
				                                                   e_ISPNetwork, // lParam
				                                                   l_isp_item.m_tree_item, // aParent,
				                                                   0  // hInsertAfter
				                                                  );
			}
		}
		// Хабы
		const StringTokenizer<string> l_hubs(l_isp.getTokens()[5], ',');
		for (auto j = l_hubs.getTokens().cbegin(); j != l_hubs.getTokens().cend() ; ++j)
		{
			if (!m_filter.empty() && Text::toLower(*j).find(Text::toLower(m_filter)) == string::npos)
				continue;
			auto& l_hub_item =  l_network_item.m_hubs[*j];
			if (!l_hub_item.m_tree_item)
			{
				l_hub_item.m_tree_item = m_ctrlTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
				                                               Text::toT(*j).c_str(),
				                                               g_ISPImage.m_flagImageCount + 5, // nImage
				                                               g_ISPImage.m_flagImageCount + 5, // nSelectedImage
				                                               0, // nState
				                                               0, // nStateMask
				                                               e_ISPHub, // lParam
				                                               !l_network_name.empty() ? l_network_item.m_tree_item : l_isp_item.m_tree_item, // aParent,
				                                               0  // hInsertAfter
				                                              );
			}
			else
			{
				p_log.step("Skip duplicate Hub [" + *j + "] Line = " + p_line);
			}
		}
	}
}

HTREEITEM PublicHubsFrame::LoadTreeItems(TStringSet& l_hubs, HTREEITEM hRoot)
{
	if (hRoot)
	{
		tstring buf;
		TV_ITEM tvi = {0};
		tvi.mask    = TVIF_TEXT;
		tvi.hItem  =  hRoot;
		if (m_ctrlTree.GetItem(&tvi))
		{
			if (tvi.lParam == e_ISPHub)
			{
				buf.resize(1000);
				m_ctrlTree.GetItemText(hRoot, &buf[0], buf.size());
				l_hubs.insert(buf);
			}
		}
		HTREEITEM hSub = m_ctrlTree.GetChildItem(hRoot);
		while (hSub)
		{
			HTREEITEM hFound = LoadTreeItems(l_hubs, hSub);
			if (hFound)
				return hFound;
			hSub = m_ctrlTree.GetNextSiblingItem(hSub);
		}
	}
	return NULL;
}

LRESULT PublicHubsFrame::onCtlColor(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	const HWND hWnd = (HWND)lParam;
	const HDC hDC = (HDC)wParam;
	if (uMsg == WM_CTLCOLORLISTBOX ||  hWnd == ctrlFilter.m_hWnd || hWnd == ctrlFilterSel.m_hWnd)
	{
		return Colors::setColor(hDC);
	}
	bHandled = FALSE;
	return FALSE;
}

LRESULT PublicHubsFrame::onColumnClickHublist(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMLISTVIEW* l = (NMLISTVIEW*)pnmh;
	if (l->iSubItem == m_ctrlHubs.getSortColumn())
	{
		if (!m_ctrlHubs.isAscending())
			m_ctrlHubs.setSort(-1, m_ctrlHubs.getSortType());
		else
			m_ctrlHubs.setSortDirection(false);
	}
	else
	{
		// BAH, sorting on bytes will break of course...oh well...later...
		if (l->iSubItem == COLUMN_USERS || l->iSubItem == COLUMN_MINSLOTS || l->iSubItem == COLUMN_MAXHUBS || l->iSubItem == COLUMN_MAXUSERS)
		{
			m_ctrlHubs.setSort(l->iSubItem, ExListViewCtrl::SORT_INT);
		}
		else if (l->iSubItem == COLUMN_RELIABILITY)
		{
			m_ctrlHubs.setSort(l->iSubItem, ExListViewCtrl::SORT_FLOAT);
		}
		else if (l->iSubItem == COLUMN_SHARED || l->iSubItem == COLUMN_MINSHARE)
		{
			m_ctrlHubs.setSort(l->iSubItem, ExListViewCtrl::SORT_BYTES);
		}
		else
		{
			m_ctrlHubs.setSort(l->iSubItem, ExListViewCtrl::SORT_STRING_NOCASE);
		}
	}
	return 0;
}

LRESULT PublicHubsFrame::onDoubleClickHublist(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	if (!checkNick())
		return 0;
		
	NMITEMACTIVATE* item = (NMITEMACTIVATE*) pnmh;
	
	if (item->iItem != -1)
		openHub(item->iItem);
		
	return 0;
}

void PublicHubsFrame::openHub(int ind) // [+] IRainman fix.
{
	RecentHubEntry r;
	r.setName(m_ctrlHubs.ExGetItemText(ind, COLUMN_NAME));
	r.setDescription(m_ctrlHubs.ExGetItemText(ind, COLUMN_DESCRIPTION));
	r.setUsers(m_ctrlHubs.ExGetItemText(ind, COLUMN_USERS));
	r.setShared(m_ctrlHubs.ExGetItemText(ind, COLUMN_SHARED));
	const string l_server = Util::formatDchubUrl(m_ctrlHubs.ExGetItemText(ind, COLUMN_SERVER));
	r.setServer(l_server);
	FavoriteManager::getInstance()->addRecent(r);
	
	HubFrame::openWindow(false, l_server);
}

LRESULT PublicHubsFrame::onEnter(int /*idCtrl*/, LPNMHDR /* pnmh */, BOOL& /*bHandled*/)
{
	if (!checkNick())
		return 0;
		
	int item = m_ctrlHubs.GetNextItem(-1, LVNI_FOCUSED);
	if (item != -1)
		openHub(item);
		
	return 0;
}

LRESULT PublicHubsFrame::onClickedConnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!checkNick())
		return 0;
		
	if (m_ctrlHubs.GetSelectedCount() >= 100) // maximum hubs per one connection
	{
		if (MessageBox(CTSTRING(PUBLIC_HUBS_WARNING), _T(" "), MB_ICONWARNING | MB_YESNO) == IDNO)
			return 0;
	}
	int i = -1;
	while ((i = m_ctrlHubs.GetNextItem(i, LVNI_SELECTED)) != -1)
		openHub(i);
		
	return 0;
}

LRESULT PublicHubsFrame::onFilterFocus(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled)
{
	bHandled = true;
	ctrlFilter.SetFocus();
	return 0;
}

LRESULT PublicHubsFrame::onAdd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!checkNick())
		return 0;
		
	int i = -1;
	while ((i = m_ctrlHubs.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		FavoriteHubEntry e;
		e.setName(m_ctrlHubs.ExGetItemText(i, COLUMN_NAME));
		e.setDescription(m_ctrlHubs.ExGetItemText(i, COLUMN_DESCRIPTION));
		e.setServer(Util::formatDchubUrl(m_ctrlHubs.ExGetItemText(i, COLUMN_SERVER)));
		//  if (!client->getPassword().empty()) // ToDo: Use SETTINGS Nick and Password
		//  {
		//      e.setNick(client->getMyNick());
		//      e.setPassword(client->getPassword());
		//  }
		FavoriteManager::getInstance()->addFavorite(e);
	}
	return 0;
}

LRESULT PublicHubsFrame::onRemoveFav(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = m_ctrlHubs.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const auto fhe = FavoriteManager::getFavoriteHubEntry(getPubServer((int)i));
		if (fhe)
		{
			FavoriteManager::getInstance()->removeFavorite(fhe);
		}
	}
	return 0;
}

LRESULT PublicHubsFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (!m_closed)
	{
		m_closed = true;
		SettingsManager::getInstance()->removeListener(this);
		WinUtil::setButtonPressed(ID_FILE_CONNECT, false);
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		WinUtil::saveHeaderOrder(m_ctrlHubs, SettingsManager::PUBLICHUBSFRAME_ORDER,
		                         SettingsManager::PUBLICHUBSFRAME_WIDTHS, COLUMN_LAST, columnIndexes, columnSizes);
		SET_SETTING(HUBS_PUBLIC_COLUMNS_SORT, m_ctrlHubs.getSortColumn());
		SET_SETTING(HUBS_PUBLIC_COLUMNS_SORT_ASC, m_ctrlHubs.isAscending());
		SET_SETTING(FLYSERVER_HUBLIST_SPLIT, m_nProportionalPos);
		bHandled = FALSE;
		return 0;
	}
}

void PublicHubsFrame::UpdateLayout(BOOL bResizeBars /* = TRUE */)
{
	if (isClosedOrShutdown())
		return;
	RECT rect;
	GetClientRect(&rect);
	// position bars and offset their dimensions
	UpdateBarsPosition(rect, bResizeBars);
	
	if (ctrlStatus.IsWindow())
	{
		CRect sr;
		int w[3];
		ctrlStatus.GetClientRect(sr);
		int tmp = (sr.Width() > 600) ? 180 : 150;
		
		w[2] = sr.right;
		w[1] = w[2] - tmp;          // Users Field start
		w[0] = w[1] - (tmp / 2);    // Hubs Field start
		
		ctrlStatus.SetParts(3, w);
	}
	
	int const comboH = 140;
	
	// listview
	CRect rc = rect;
	//rc.top += 2; //[~] Sergey Shuhskanov
	rc.bottom -= 56;
	m_ctrlHubs.MoveWindow(rc);
	
	// filter box
	rc = rect;
	rc.left += 4;
	rc.top = rc.bottom - 52;
	rc.bottom = rc.top + 46;
	rc.right -= 100;
	rc.right -= ((rc.right - rc.left) / 2) + 1;
	ctrlFilterDesc.MoveWindow(rc);
	
	// filter edit
	rc.top += 16;
	rc.bottom -= 8;
	rc.left += 8;
	rc.right -= ((rc.right - rc.left - 4) / 3);
	ctrlFilter.MoveWindow(rc);
	
	//filter sel
	rc.bottom += comboH;
	rc.right += ((rc.right - rc.left - 12) / 2) ;
	rc.left += ((rc.right - rc.left + 8) / 3) * 2;
	ctrlFilterSel.MoveWindow(rc);
	
	
	rect.bottom -= 60;
	SetSplitterRect(&rect);
}

bool PublicHubsFrame::checkNick()
{
	if (SETTING(NICK).empty())
	{
		MessageBox(CTSTRING(ENTER_NICK), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_ICONSTOP | MB_OK);
		return false;
	}
	return true;
}
void PublicHubsFrame::updateTree()
{
	loadISPHubs();
}
void PublicHubsFrame::updateList()
{
	//CLockRedraw<> l_lock_draw(m_ctrlHubs);
	m_ctrlHubs.DeleteAllItems();
	users = 0;
	visibleHubs = 0;
	
	double size = -1;
	FilterModes mode = NONE;
	
	int sel = ctrlFilterSel.GetCurSel();
	
	bool doSizeCompare = parseFilter(mode, size);
	
	auto cnt = m_ctrlHubs.GetItemCount();
	for (auto i = m_hubs.cbegin(); i != m_hubs.cend(); ++i)
	{
		if (matchFilter(*i, sel, doSizeCompare, mode, size))
		{
			TStringList l;
			l.resize(COLUMN_LAST);
			l[COLUMN_NAME] = Text::toT(i->getName());
			string l_description = i->getDescription();
			boost::replace_all(l_description, ".px.", "");
			l[COLUMN_DESCRIPTION] = Text::toT(l_description);
			l[COLUMN_USERS] = Util::toStringW(i->getUsers());
			l[COLUMN_SERVER] = Text::toT(i->getServer());
			l[COLUMN_COUNTRY] = Text::toT(i->getCountry()); // !SMT!-IP
			l[COLUMN_SHARED] = Util::formatBytesW(i->getShared());
			l[COLUMN_MINSHARE] = Util::formatBytesW(i->getMinShare());
			l[COLUMN_MINSLOTS] = Util::toStringW(i->getMinSlots());
			l[COLUMN_MAXHUBS] = Util::toStringW(i->getMaxHubs());
			l[COLUMN_MAXUSERS] = Util::toStringW(i->getMaxUsers());
			l[COLUMN_RELIABILITY] = Util::toStringW(i->getReliability());
			l[COLUMN_RATING] = Text::toT(i->getRating());
			const auto l_country = i->getCountry();
			dcassert(!l_country.empty());
			const auto l_index_country = WinUtil::getFlagIndexByName(l_country.c_str());
			//const auto l_index =
			m_ctrlHubs.insert(cnt++, l, l_index_country); // !SMT!-IP
			
			/*
			LVITEM lvItem = { 0 };
			        lvItem.mask = LVIF_IMAGE;
			        lvItem.iItem = l_index;
			        lvItem.iImage = isOnline(i->getServer()) ? 0 : 1;
			        m_ctrlHubs.SetItem(&lvItem);
			*/
			visibleHubs++;
			users += i->getUsers();
		}
	}
	
	m_ctrlHubs.resort();
	
	updateStatus();
}

void PublicHubsFrame::updateStatus()
{
	ctrlStatus.SetText(1, (TSTRING(HUBS) + _T(": ") + Util::toStringW(visibleHubs)).c_str());
	ctrlStatus.SetText(2, (TSTRING(USERS) + _T(": ") + Util::toStringW(users)).c_str());
}

LRESULT PublicHubsFrame::onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	if (wParam == SET_TEXT)
	{
		tstring* x = (tstring*)lParam;
		ctrlStatus.SetText(0, (*x).c_str());
		delete x;
	}
	return 0;
}

LRESULT PublicHubsFrame::onFilterChar(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	tstring tmp;
	WinUtil::GetWindowText(tmp, ctrlFilter);
	m_filter = Text::fromT(tmp);
	if (wParam == VK_RETURN)
	{
		updateList();
	}
	else
	{
		updateTree();
		bHandled = FALSE;
	}
	return 0;
}

LRESULT PublicHubsFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (reinterpret_cast<HWND>(wParam) == m_ctrlHubs /*&& ctrlHubs.GetSelectedCount() == 1*/)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		CRect rc;
		m_ctrlHubs.GetHeader().GetWindowRect(&rc);
		if (PtInRect(&rc, pt))
		{
			return 0;
		}
		if (pt.x == -1 && pt.y == -1)
		{
			WinUtil::getContextMenuPos(m_ctrlHubs, pt);
		}
		int status = m_ctrlHubs.GetSelectedCount() > 0 ? MFS_ENABLED : MFS_DISABLED;
		hubsMenu.EnableMenuItem(IDC_CONNECT, status);
		hubsMenu.EnableMenuItem(IDC_ADD, status);
		hubsMenu.EnableMenuItem(IDC_REM_AS_FAVORITE, status);
		if (m_ctrlHubs.GetSelectedCount() > 1)
		{
			hubsMenu.EnableMenuItem(IDC_COPY_HUB, MFS_DISABLED);
		}
		else
		{
			int i = -1;
			while ((i = m_ctrlHubs.GetNextItem(i, LVNI_SELECTED)) != -1)
			{
				const auto fhe = FavoriteManager::getFavoriteHubEntry(getPubServer((int)i));
				if (fhe)
				{
					hubsMenu.EnableMenuItem(IDC_ADD, MFS_DISABLED);
					hubsMenu.EnableMenuItem(IDC_REM_AS_FAVORITE, MFS_ENABLED);
				}
				else
				{
					hubsMenu.EnableMenuItem(IDC_ADD, MFS_ENABLED);
					hubsMenu.EnableMenuItem(IDC_REM_AS_FAVORITE, MFS_DISABLED);
				}
			}
			hubsMenu.EnableMenuItem(IDC_COPY_HUB, status);
		}
		hubsMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		return TRUE;
	}
	
	bHandled = FALSE;
	return FALSE;
}

LRESULT PublicHubsFrame::onCopyHub(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (m_ctrlHubs.GetSelectedCount() == 1)
	{
		LocalArray<TCHAR, 256> buf;
		int i = m_ctrlHubs.GetNextItem(-1, LVNI_SELECTED);
		m_ctrlHubs.GetItemText(i, COLUMN_SERVER, buf.data(), 256);
		WinUtil::setClipboard(buf.data());
	}
	return 0;
}


bool PublicHubsFrame::parseFilter(FilterModes& mode, double& size)
{
	string::size_type start = string::npos;
	string::size_type end = string::npos;
	int64_t multiplier = 1;
	
	if (m_filter.empty()) return false;
	if (m_filter.compare(0, 2, ">=", 2) == 0)
	{
		mode = GREATER_EQUAL;
		start = 2;
	}
	else if (m_filter.compare(0, 2, "<=", 2) == 0)
	{
		mode = LESS_EQUAL;
		start = 2;
	}
	else if (m_filter.compare(0, 2, "==", 2) == 0)
	{
		mode = EQUAL;
		start = 2;
	}
	else if (m_filter.compare(0, 2, "!=", 2) == 0)
	{
		mode = NOT_EQUAL;
		start = 2;
	}
	else if (m_filter[0] == '<')
	{
		mode = LESS;
		start = 1;
	}
	else if (m_filter[0] == '>')
	{
		mode = GREATER;
		start = 1;
	}
	else if (m_filter[0] == '=')
	{
		mode = EQUAL;
		start = 1;
	}
	
	if (start == string::npos)
		return false;
	if (m_filter.length() <= start)
		return false;
		
	if ((end = Util::findSubString(m_filter, "TiB")) != tstring::npos)
	{
		multiplier = 1024LL * 1024LL * 1024LL * 1024LL;
	}
	else if ((end = Util::findSubString(m_filter, "GiB")) != tstring::npos)
	{
		multiplier = 1024 * 1024 * 1024;
	}
	else if ((end = Util::findSubString(m_filter, "MiB")) != tstring::npos)
	{
		multiplier = 1024 * 1024;
	}
	else if ((end = Util::findSubString(m_filter, "KiB")) != tstring::npos)
	{
		multiplier = 1024;
	}
	else if ((end = Util::findSubString(m_filter, "TB")) != tstring::npos)
	{
		multiplier = 1000LL * 1000LL * 1000LL * 1000LL;
	}
	else if ((end = Util::findSubString(m_filter, "GB")) != tstring::npos)
	{
		multiplier = 1000 * 1000 * 1000;
	}
	else if ((end = Util::findSubString(m_filter, "MB")) != tstring::npos)
	{
		multiplier = 1000 * 1000;
	}
	else if ((end = Util::findSubString(m_filter, "kB")) != tstring::npos)
	{
		multiplier = 1000;
	}
	else if ((end = Util::findSubString(m_filter, "B")) != tstring::npos)
	{
		multiplier = 1;
	}
	
	
	if (end == string::npos)
	{
		end = m_filter.length();
	}
	
	string tmpSize = m_filter.substr(start, end - start);
	size = Util::toDouble(tmpSize) * multiplier;
	
	return true;
}

bool PublicHubsFrame::matchFilter(const HubEntry& entry, const int& sel, bool doSizeCompare, const FilterModes& mode, const double& size)
{
	if (m_filter.empty())
		return true;
		
	double entrySize = 0;
	string entryString;
	
	switch (sel)
	{
		case COLUMN_NAME:
			entryString = entry.getName();
			doSizeCompare = false;
			break;
		case COLUMN_DESCRIPTION:
			entryString = entry.getDescription();
			doSizeCompare = false;
			break;
		case COLUMN_USERS:
			entrySize = entry.getUsers();
			break;
		case COLUMN_SERVER:
			entryString = entry.getServer();
			doSizeCompare = false;
			break;
		case COLUMN_COUNTRY:
			entryString = entry.getCountry();
			doSizeCompare = false;
			break;
		case COLUMN_SHARED:
			entrySize = (double)entry.getShared();
			break;
		case COLUMN_MINSHARE:
			entrySize = (double)entry.getMinShare();
			break;
		case COLUMN_MINSLOTS:
			entrySize = entry.getMinSlots();
			break;
		case COLUMN_MAXHUBS:
			entrySize = entry.getMaxHubs();
			break;
		case COLUMN_MAXUSERS:
			entrySize = entry.getMaxUsers();
			break;
		case COLUMN_RELIABILITY:
			entrySize = entry.getReliability();
			break;
		case COLUMN_RATING:
			entryString = entry.getRating();
			doSizeCompare = false;
			break;
		default:
			break;
	}
	
	bool insert = false;
	if (doSizeCompare)
	{
		switch (mode)
		{
			case EQUAL:
				insert = (size == entrySize);
				break;
			case GREATER_EQUAL:
				insert = (size <=  entrySize);
				break;
			case LESS_EQUAL:
				insert = (size >=  entrySize);
				break;
			case GREATER:
				insert = (size < entrySize);
				break;
			case LESS:
				insert = (size > entrySize);
				break;
			case NOT_EQUAL:
				insert = (size != entrySize);
				break;
		}
	}
	else
	{
		if (sel >= COLUMN_LAST)
		{
			if (Util::findSubString(entry.getName(), m_filter) != string::npos ||
			        Util::findSubString(entry.getDescription(), m_filter) != string::npos ||
			        Util::findSubString(entry.getServer(), m_filter) != string::npos ||
			        Util::findSubString(entry.getCountry(), m_filter) != string::npos ||
			        Util::findSubString(entry.getRating(), m_filter) != string::npos)
			{
				insert = true;
			}
		}
		if (Util::findSubString(entryString, m_filter) != string::npos)
			insert = true;
	}
	
	return insert;
}

void PublicHubsFrame::on(SettingsManagerListener::Repaint)
{
	dcassert(!ClientManager::isShutdown());
	if (!ClientManager::isShutdown())
	{
		if (m_ctrlHubs.isRedraw())
		{
			RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
		}
	}
}

LRESULT PublicHubsFrame::onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled */)
{
	LPNMLVCUSTOMDRAW cd = reinterpret_cast<LPNMLVCUSTOMDRAW>(pnmh);
	
	switch (cd->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
			return CDRF_NOTIFYITEMDRAW;
			
		case CDDS_ITEMPREPAINT:
		{
			if (isOnline(getPubServer((int)cd->nmcd.dwItemSpec)))
			{
				cd->clrTextBk = HLS_TRANSFORM(((cd->clrTextBk == CLR_DEFAULT) ? ::GetSysColor(COLOR_WINDOW) : cd->clrTextBk), -9, 0);
			}
			return CDRF_NEWFONT;
		}
	}
	return CDRF_DODEFAULT;
}
/*
//  return CDRF_DODEFAULT;
#ifdef SCALOLAZ_USE_COLOR_HUB_IN_FAV
    LPNMLVCUSTOMDRAW cd = reinterpret_cast<LPNMLVCUSTOMDRAW>(pnmh);

    switch (cd->nmcd.dwDrawStage)
    {
        case CDDS_PREPAINT:
            return CDRF_NOTIFYITEMDRAW;

        case CDDS_ITEMPREPAINT:
        {
            cd->clrText = Colors::g_textColor;
            const auto fhe = FavoriteManager::getFavoriteHubEntry(getPubServer((int)cd->nmcd.dwItemSpec));
            if (fhe)
            {
                if (fhe->getConnect())
                {
                    cd->clrTextBk = SETTING(HUB_IN_FAV_CONNECT_BK_COLOR);
                }
                else
                {
                    cd->clrTextBk = SETTING(HUB_IN_FAV_BK_COLOR);
                }
            }
            return CDRF_NEWFONT;
        }
        return CDRF_NEWFONT | CDRF_NOTIFYSUBITEMDRAW;
    }
}
return CDRF_DODEFAULT;
#endif // SCALOLAZ_USE_COLOR_HUB_IN_FAV
}
*/


/**
 * @file
 * $Id: PublicHubsFrm.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
