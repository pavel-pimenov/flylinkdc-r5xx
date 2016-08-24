/*
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
#include "ChatCtrl.h"
#include "MainFrm.h"

#ifdef IRAINMAN_INCLUDE_SMILE
#include "../GdiOle/GDIImageOle.h"

#include "AGEmotionSetup.h"


#define MAX_EMOTICONS 48
#endif // IRAINMAN_INCLUDE_SMILE

static const TCHAR g_BadUrlSymbol[] = { _T(' '), _T('\"'), _T('<'), _T('>'),
                                        _T('['), _T(']') // TODO used in BB codes, needs refactoring
                                        /* _T('^'),_T('`'),_T('{'),_T('|'),_T('}'),_T('*'),_T('\'')*/
                                      };// [+]IRainman http://ru.wikipedia.org/wiki/Url
static const TCHAR g_GoodBorderNickSymbol[] = { _T(' '), _T('\"'), _T('<'), _T('>'),/* _T('['), _T(']'),*/
                                                _T(','), _T('.'),  _T('('), _T(')'), _T('!'), _T('?'),
                                                _T('*'), _T(':'),  _T(';'), _T('%'), _T('+'), _T('-')/*,
                                                _T('_')*/
                                              };

static const Tags g_AllLinks[] =
{
	Tags(_T("dchub://")),
	Tags(_T("nmdcs://")),
	Tags(_T("magnet:?")),
	Tags(_T("adc://")),
	Tags(_T("adcs://")),
	EXT_URL_LIST(),
};


#ifdef IRAINMAN_USE_BB_CODES
static const Tags g_StartBBTag[] =
{
	Tags(_T("[code]")), // TODO отключать форматирование
	Tags(_T("[b]")),
	Tags(_T("[i]")),
	Tags(_T("[u]")),
	Tags(_T("[s]")),
	Tags(_T("[img]")),
	Tags(_T("[color=")),
};

static const Tags g_EndBBTag[] =
{
	Tags(_T("[/code]")), // TODO отключать форматирование
	Tags(_T("[/b]")), //-V112
	Tags(_T("[/i]")), //-V112
	Tags(_T("[/u]")), //-V112
	Tags(_T("[/s]")), //-V112
	Tags(_T("[/img]")),
	Tags(_T("[/color]")),
};

//struct Interval
//{
//	Interval (long s = 0, long e = 0) : start(s), end(e) {};
//	long start, end;
//};
//typedef vector<Interval> Intervals;
//typedef Intervals::const_iterator CIterIntervals;
#endif // IRAINMAN_USE_BB_CODES

tstring ChatCtrl::g_sSelectedLine;
tstring ChatCtrl::g_sSelectedText;
tstring ChatCtrl::g_sSelectedIP;
tstring ChatCtrl::g_sSelectedUserName;
tstring ChatCtrl::g_sSelectedURL;

ChatCtrl::ChatCtrl() : m_boAutoScroll(true), m_is_cache_chat_empty(false), m_is_out_of_memory_for_smile(false), m_chat_cache_length(0) //, m_Client(nullptr)
#ifdef IRAINMAN_INCLUDE_SMILE
	, m_pRichEditOle(NULL), /*m_pOleClientSite(NULL),*/ m_pStorage(NULL), m_lpLockBytes(NULL), m_Ref(0)
#endif
{
	//m_cs_chat_cache = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
	//m_hStandardCursor = LoadCursor(NULL, IDC_IBEAM);
	//m_hHandCursor = LoadCursor(NULL, IDC_HAND);
}

ChatCtrl::~ChatCtrl()
{
#ifdef IRAINMAN_INCLUDE_SMILE
	safe_release(m_pStorage);
	safe_release(m_lpLockBytes);
	safe_release(m_pRichEditOle);
#endif // IRAINMAN_INCLUDE_SMILE
}

void ChatCtrl::Initialize()
{
#ifdef IRAINMAN_INCLUDE_SMILE
	m_pRichEditOle = GetOleInterface();
	dcassert(m_pRichEditOle);
	if (m_pRichEditOle)
	{
		const SCODE sc = ::CreateILockBytesOnHGlobal(NULL, TRUE, &m_lpLockBytes);
		if (sc == S_OK && m_lpLockBytes)
		{
			::StgCreateDocfileOnILockBytes(m_lpLockBytes,
			                               STGM_SHARE_EXCLUSIVE | STGM_CREATE | STGM_READWRITE, 0, &m_pStorage);
		}
	}
	SetOleCallback(this);
#endif // IRAINMAN_INCLUDE_SMILE
}

void ChatCtrl::AdjustTextSize()
{
	// [!] IRainman fix.
	const auto l_cur_size = GetWindowTextLength();
	const auto l_overhead = l_cur_size - SETTING(CHATBUFFERSIZE);
	if (l_overhead > 1000)
	{
		CLockRedraw<> l_lock_draw(m_hWnd);
		for (auto i = m_URLMap.begin(); i != m_URLMap.end();)
		{
			i->first -= l_overhead;
			if (i->first < 0)
				m_URLMap.erase(i++);
			else
				++i;
		}
		SetSel(0, l_overhead);
		ReplaceSel(_T(""));
#ifdef _DEBUG
		const auto l_new_size = GetWindowTextLength();
		LogManager::message("ChatCtrl::AdjustTextSize() l_cur_size = " + Util::toString(l_cur_size) + " delta = " + Util::toString(l_cur_size - l_new_size));
#endif
	}
	// [~] IRainman fix.
}
//================================================================================================================================
ChatCtrl::CFlyChatCache::CFlyChatCache(const Identity& p_id, const bool bMyMess, const bool bThirdPerson,
                                       const tstring& sExtra, const tstring& sMsg, const CHARFORMAT2& p_cf, bool bUseEmo,
                                       bool p_is_remove_rn /*= true*/) :
	CFlyChatCacheTextOnly(p_id.getNickT(),
	                      bMyMess,
	                      p_id.isBotOrHub(),
	                      sMsg,
	                      p_cf),
	m_bThirdPerson(bThirdPerson),
	m_Extra(sExtra),
	m_bUseEmo(bUseEmo),
	m_is_disable_style(false),
	m_is_url(false)
{
	dcassert(!ClientManager::isShutdown());
	if (!ClientManager::isShutdown())
	{
		m_bUseEmo = bUseEmo || !ClientManager::isStartup(); // Пока запускаемся - аним-смайлы отрубаем
		if (p_is_remove_rn)
			Text::normalizeStringEnding(m_Msg);
		m_Msg += '\n';
		if (!CAGEmotionSetup::g_pEmotionsSetup || CompatibilityManager::isWine())
		{
			m_bUseEmo = false;
		}
		
		for (size_t i = 0; i < _countof(g_AllLinks); i++)
		{
			if (m_Msg.find(g_AllLinks[i].tag) != tstring::npos)
			{
				m_is_url = true;
				m_bUseEmo = false;
				break;
			}
		}
		
		m_is_op = p_id.isOp();
		m_is_ban = false;
		if (!bThirdPerson)
		{
			m_isFavorite = !bMyMess && FavoriteManager::isFavoriteUser(p_id.getUser(), m_is_ban);
		}
	}
}
//================================================================================================================================
ChatCtrl::CFlyChatCacheTextOnly::CFlyChatCacheTextOnly(const tstring& p_nick,
                                                       const bool p_is_my_mess,
                                                       const bool p_is_real_user,
                                                       const tstring& p_msg,
                                                       const CHARFORMAT2& p_cf):
	m_Msg(p_msg),
	m_Nick(p_nick),
	m_bMyMess(p_is_my_mess),
	m_isRealUser(p_is_real_user),
	m_cf(p_cf)
{
}
//================================================================================================================================
void ChatCtrl::restore_chat_cache()
{
	CLockRedraw<true> l_lock_draw(m_hWnd);
	CWaitCursor l_cursor_wait; //-V808
	{
		//CFlyReadLock(*m_cs_chat_cache);
#if 0
		for (int i = 0; i < 3000; ++i)
		{
			ChatCtrl::CFlyChatCache l_message(ClientManager::getFlylinkDCIdentity(),
			                                  false,
			                                  true,
			                                  _T('[') + Text::toT(Util::getShortTimeString()) + _T("] "),
			                                  _T("Test!Test!Test!Test!Test!Test!Test!Test!Test!Test!Test!Test!Test!Test!Test!Test!Test!Test!Test!Test!Test!Test!Test!Test!Test!Test!Test!Test!Test!Test!Test!"),
			                                  Colors::g_ChatTextOldHistory,
			                                  false);
			l_message.m_Nick = _T("FlylinkDC-Debug-TEST");
			m_chat_cache.push_back(l_message);
		}
#endif
		if (m_is_cache_chat_empty == false)
		{
			CLockRedraw<true> l_lock_redraw(*this);
			m_is_cache_chat_empty = true;
			int l_count = m_chat_cache.size();
			for (auto i = m_chat_cache.begin(); i != m_chat_cache.end(); ++i)
			{
			
				if (l_count-- > 40) // Отрубаем смайлы и стиль на старых записях
				{
					i->m_bUseEmo = false;
					i->m_is_disable_style = true;
				}
				AppendText(*i, false);
			}
		}
		{
			//CFlyWriteLock(*m_cs_chat_cache);
			m_chat_cache.clear();
			m_chat_cache_length = 0;
		}
	}
	m_is_out_of_memory_for_smile = false; // Снова пытамся вставлять смайлы
}
//================================================================================================================================
void ChatCtrl::insertAndFormat(const tstring & text, CHARFORMAT2 cf, bool p_is_disable_style, LONG& p_begin, LONG& p_end)
{
	dcassert(!ClientManager::isShutdown());
	if (ClientManager::isShutdown())
		return;
	dcassert(!text.empty());
	if (!text.empty())
	{
		p_begin = p_end = GetTextLengthEx(GTL_NUMCHARS);
		SetSel(p_end, p_end);
		ReplaceSel(text.c_str());
		p_end = GetTextLengthEx(GTL_NUMCHARS);
		SetSel(p_begin, p_end);
		SetSelectionCharFormat(cf);
	}
}
//================================================================================================================================
void ChatCtrl::AppendText(const CFlyChatCache& p_message, bool p_is_lock_redraw)
//    const Identity& p_id, const bool bMyMess, const bool bThirdPerson, const tstring& sExtra, const tstring& sMsg, const CHARFORMAT2& cf
//, bool bUseEmo/* = true*/
{
	dcassert(!ClientManager::isShutdown());
	if (ClientManager::isShutdown())
		return;
	{
		//CFlyWriteLock(*m_cs_chat_cache);
		if (m_is_cache_chat_empty == false)
		{
			m_chat_cache.push_back(p_message);
			m_chat_cache_length += p_message.length();
			if (m_chat_cache_length + 1000 > SETTING(CHATBUFFERSIZE))
			{
				m_chat_cache_length -= m_chat_cache.front().length();
				m_chat_cache.pop_front();
			}
			return;
		}
	}
	unique_ptr<CLockRedraw<true>> l_ptr_lock_draw;
	if (p_is_lock_redraw)
	{
		l_ptr_lock_draw = std::make_unique<CLockRedraw<true> >(m_hWnd);
	}
	LONG lSelBeginSaved, lSelEndSaved;
	GetSel(lSelBeginSaved, lSelEndSaved);
	POINT l_cr;
	GetScrollPos(&l_cr);
	
	LONG lSelBegin = 0;
	LONG lSelEnd = 0;
	
	// Insert extra info and format with default style
	if (!p_message.m_Extra.empty())
	{
		insertAndFormat(p_message.m_Extra, Colors::g_TextStyleTimestamp, p_message.m_is_disable_style, lSelBegin, lSelEnd);
		
		PARAFORMAT2 pf;
		memzero(&pf, sizeof(PARAFORMAT2));
		pf.dwMask = PFM_STARTINDENT;
		pf.dxStartIndent = 0;
		SetParaFormat(pf);
	}
	
	tstring sText = p_message.m_Msg;
	//const tstring& sAuthor = p_message.m_Nick;
	//dcassert(!sAuthor.empty());
	if (p_message.m_Nick.empty())
	{
		// TODO: Needs extra format for program message?
	}
	else if (p_message.m_bThirdPerson)
	{
		if (p_message.m_is_disable_style)
		{
			insertAndFormat(_T("* ") + p_message.m_Nick + _T(" "), p_message.m_cf, p_message.m_is_disable_style, lSelBegin, lSelEnd);
		}
		else
		{
			const CHARFORMAT2& currentCF =
			    p_message.m_bMyMess ? Colors::g_ChatTextMyOwn :
			    BOOLSETTING(BOLD_AUTHOR_MESS) ? Colors::g_TextStyleBold :
			    p_message.m_cf;
			insertAndFormat(_T("* "), p_message.m_cf, p_message.m_is_disable_style, lSelBegin, lSelEnd);
			insertAndFormat(p_message.m_Nick, currentCF, p_message.m_is_disable_style, lSelBegin, lSelEnd);
			insertAndFormat(_T(" "), p_message.m_cf, p_message.m_is_disable_style, lSelBegin, lSelEnd);
		}
	}
	else
	{
		static const tstring g_open = _T("<");
		static const tstring g_close = _T("> ");
		if (p_message.m_is_disable_style)
		{
			insertAndFormat(g_open + p_message.m_Nick + g_close, p_message.m_cf, p_message.m_is_disable_style, lSelBegin, lSelEnd);
		}
		else
		{
			const CHARFORMAT2& currentCF =
			    p_message.m_bMyMess ? Colors::g_TextStyleMyNick :
			    p_message.m_isFavorite ? (p_message.m_is_ban ? Colors::g_TextStyleFavUsersBan : Colors::g_TextStyleFavUsers) :
				    p_message.m_is_op ? Colors::g_TextStyleOPs :
				    BOOLSETTING(BOLD_AUTHOR_MESS) ? Colors::g_TextStyleBold :
				    p_message.m_cf;
			//dcassert(sizeof(currentCF) == sizeof(p_message.m_cf));
			//if (memcmp(&currentCF, &p_message.m_cf,sizeof(currentCF)) == 0)
			//{
			//  insertAndFormat(g_open + p_message.m_Nick + g_close, p_message.m_cf, p_message.m_is_disable_style, lSelBegin, lSelEnd);
			//}
			//else
		{
				insertAndFormat(g_open, p_message.m_cf, p_message.m_is_disable_style, lSelBegin, lSelEnd);
				insertAndFormat(p_message.m_Nick, currentCF, p_message.m_is_disable_style, lSelBegin, lSelEnd);
				insertAndFormat(g_close, p_message.m_cf, p_message.m_is_disable_style, lSelBegin, lSelEnd);
			}
		}
	}
	
	// Ensure that EOLs will be always same
#ifdef IRAINMAN_INCLUDE_SMILE
	// Если кончилась память на GDI - даже не пытаемся создавать смайлы (TODO - зачистить после полной загрузки кеша);
	extern DWORD g_GDI_count;
	bool bUseEmo = p_message.m_bUseEmo && m_is_out_of_memory_for_smile == false && g_GDI_count < 8000;
	if (g_GDI_count >= 8000)
	{
		LogManager::message("[!] GDI count >= 8000 - disable smiles!");
	}
	if (bUseEmo)
	{
		const CAGEmotion::Array& Emoticons = CAGEmotionSetup::g_pEmotionsSetup->getEmoticonsArray();
		uint8_t l_count_smiles = 0;
		while (m_is_out_of_memory_for_smile == false)
		{
			if (l_count_smiles < MAX_EMOTICONS)
			{
				auto l_pos = tstring::npos;
				auto l_min_pos = tstring::npos;
				CAGEmotion* pFoundEmotion = nullptr;
				tstring l_cur_smile;
				for (auto pEmotion = Emoticons.cbegin(); pEmotion != Emoticons.cend(); ++pEmotion)
				{
					const auto& l_smile = (*pEmotion)->getEmotionText();
					l_pos = sText.find(l_smile);
					// TODO Иногда бывает смайл BD он встречается в TTH - проверим это..
					// найти в тексте TTH где по краям пробелы и заменить его на ссылку
					// пример
					// magnet:?xl=0&dn=OZGC4NAHZZV3TCDE5X4H5TUXIRJY4DJ63FK6BZQ&xt=urn:tree:tiger:OZGC4NAHZZV3TCDE5X4H5TUXIRJY4DJ63FK6BZQ
					if (l_pos != tstring::npos)
					{
						if (l_pos < l_min_pos)
						{
							//if(sText.length() - l_pos  > l_smile.size() + 1
							//   && Util::isTTHChar(sText[l_pos]) && )
							l_min_pos = l_pos;
							l_cur_smile   = l_smile.c_str();
							pFoundEmotion = (*pEmotion);
						}
					}
				}
				l_pos = l_min_pos;
				if (pFoundEmotion && l_pos != tstring::npos) // /*&& (pFoundEmotion->getEmotionBmp() || pFoundEmotion->getEmotionBmpPath().size())*/
				{
					if (l_pos)
					{
						AppendTextOnly(sText.substr(0, l_pos), p_message);
					}
					lSelEnd = GetTextLengthEx(GTL_NUMCHARS);
					SetSel(lSelEnd, lSelEnd);
					
					IOleClientSite *pOleClientSite = nullptr;
					
					if (!m_pRichEditOle)
						Initialize();
						
					if (m_pRichEditOle &&
					        SUCCEEDED(m_pRichEditOle->GetClientSite(&pOleClientSite))
					        && pOleClientSite)
					{
						IOleObject *pObject = pFoundEmotion->GetImageObject(BOOLSETTING(CHAT_ANIM_SMILES), pOleClientSite, m_pStorage, MainFrame::getMainFrame()->m_hWnd, WM_ANIM_CHANGE_FRAME,
						                                                    p_message.m_bMyMess ? Colors::g_ChatTextMyOwn.crBackColor : Colors::g_ChatTextGeneral.crBackColor);
						if (pObject)
						{
							CImageDataObject::InsertBitmap(m_hWnd, m_pRichEditOle, pOleClientSite, m_pStorage, pObject, m_is_out_of_memory_for_smile);
							if (m_is_out_of_memory_for_smile == false)
							{
								l_count_smiles++;
							}
							else
							{
							}
							safe_release(pObject);
							safe_release(pOleClientSite);
							safe_release(m_pRichEditOle);
						}
						else
						{
							AppendTextOnly(sText.substr(l_pos, l_cur_smile.size()), p_message);
						}
						if (!l_cur_smile.empty())
							sText = sText.substr(l_pos + l_cur_smile.size());
					}
					else if (!l_cur_smile.empty())
					{
						AppendTextOnly(sText.substr(l_pos, l_cur_smile.size()), p_message);
						sText = sText.substr(l_pos + l_cur_smile.size());
					}
				}
				else
				{
					if (!sText.empty())
					{
						AppendTextOnly(sText, p_message);
					}
					break;
				}
			}
			else
			{
				if (!sText.empty())
				{
					AppendTextOnly(sText, p_message);
				}
				break;
			}
		}
	}
	else
#endif // IRAINMAN_INCLUDE_SMILE
	{
		AppendTextOnly(sText, p_message);
	}
	SetSel(lSelBeginSaved, lSelEndSaved);
	GoToEnd(l_cr, false);
}

void ChatCtrl::AppendTextOnly(const tstring& sText, const CFlyChatCacheTextOnly& p_message)
{
	// const tstring& sAuthor, const bool bMyMess, const bool bRealUser, const CHARFORMAT2& cf
	dcassert(!ClientManager::isShutdown());
	if (ClientManager::isShutdown())
		return;
	PARAFORMAT2 pf;
	memzero(&pf, sizeof(PARAFORMAT2));
	pf.dwMask = PFM_STARTINDENT;
	pf.dxStartIndent = 0;
	
	// Insert text at the end
	LONG lSelBegin = GetTextLengthEx(GTL_NUMCHARS);
	LONG lSelEnd = lSelBegin;
	SetSel(lSelEnd, lSelEnd);
	ReplaceSel(sText.c_str());
	
	// Set text format
	LONG lMyNickStart = -1, lMyNickEnd = -1;
	CAtlString sMsgLower(WinUtil::toAtlString(sText));
	sMsgLower.MakeLower();
	
	//[!]IRainman optimize
	lSelEnd = GetTextLengthEx(GTL_NUMCHARS); // Часто встречается GetTextLengthEx(GTL_NUMCHARS)
	SetSel(lSelBegin, lSelEnd);
	auto cfTemp = p_message.m_bMyMess ? Colors::g_ChatTextMyOwn : p_message.m_cf;
	SetSelectionCharFormat(cfTemp);
	//[~]IRainman optimize
	
	if ((p_message.m_isRealUser || BOOLSETTING(FORMAT_BOT_MESSAGE)) && !p_message.m_Nick.empty())
	{
#ifdef IRAINMAN_INCLUDE_TEXT_FORMATTING
		// This is not 100% working, but most of the time it does the job decently enough
		// technically if problems happened to appear, they'd most likely appear in MOTD's,
		// bot messages and ASCII pics - Crise
		if (BOOLSETTING(FORMAT_BIU))
		{
			static const TCHAR rtf[] = { _T('*'), _T('_'), _T('/') };
			for (size_t i = 0; i < _countof(rtf); i++)
			{
				LONG rtfStart = sMsgLower.Find(rtf[i], 0);
				while (rtfStart != -1)
				{
					LONG rtfEnd;
					LONG rtfEndTerminate = sMsgLower.Find(rtf[i], rtfStart + 1);
					if (rtfEndTerminate > rtfStart)
					{
						rtfEnd = rtfEndTerminate;
					}
					else
					{
						rtfEnd = _tcslen(sMsgLower); //-V103
					}
					
					// No closing, no formatting...
					if (rtfEnd == rtfEndTerminate && (rtfEnd - rtfStart > 2))
					{
						CHARFORMAT2 temp = cf;
						temp.dwMask = CFM_BOLD | CFM_UNDERLINE | CFM_ITALIC;
						
						switch (i)
						{
							case 0:// Bold
								temp.dwEffects |= CFE_BOLD;
								break;
							case 1:// Underline
								temp.dwEffects |= CFE_UNDERLINE;
								break;
							case 2:// Italic
								temp.dwEffects |= CFE_ITALIC;
								break;
						}
						// Set formatting to selection
						SetSel(lSelBegin + rtfStart, lSelBegin + rtfEnd + 1);
						SetSelectionCharFormat(temp);
					}
					rtfStart = sMsgLower.Find(rtf[i], rtfEnd + 1);
				}
			}
		}
#endif // IRAINMAN_INCLUDE_TEXT_FORMATTING
		// BB codes support http://ru.wikipedia.org/wiki/BbCode
		AppendTextParseBB(sMsgLower, p_message, lSelBegin);
	}
	
	AppendTextParseURL(sMsgLower, p_message, lSelBegin);
	
	if (p_message.m_Nick.empty()) // [+] IRainman fix.
		return;
		
	// Zvyrazneni vsech vyskytu vlastniho nicku
	lSelEnd = GetTextLengthEx(GTL_NUMCHARS);
	LONG lSearchFrom = 0;
	
	while (!p_message.m_bMyMess && !m_MyNickLower.IsEmpty())
	{
		lMyNickStart = sMsgLower.Find(m_MyNickLower, lSearchFrom);
		if (lMyNickStart < 0)
			break;
		// [!] SSA - get Previous symbol.
		if (lMyNickStart > 0 && !isGoodNickBorderSymbol(sMsgLower.GetAt(lMyNickStart - 1)))
			break;
			
		lMyNickEnd = lMyNickStart + m_MyNickLower.GetLength();
		
		// [!] SSA - get Last symbol.
		if (lMyNickEnd < sMsgLower.GetLength() - 1 && !isGoodNickBorderSymbol(sMsgLower.GetAt(lMyNickEnd)))
			break;
			
		SetSel(lSelBegin + lMyNickStart, lSelBegin + lMyNickEnd);
		
		auto tmpCF = Colors::g_TextStyleMyNick;
		SetSelectionCharFormat(tmpCF);
		lSearchFrom = lMyNickEnd;
		
		PLAY_SOUND(SOUND_CHATNAMEFILE);
	}
	
	// Zvyrazneni vsech vyskytu nicku Favorite useru
	lSelEnd = GetTextLengthEx(GTL_NUMCHARS);
	{
		FavoriteManager::LockInstanceUsers lockedInstance;
		const auto& l_fav_users = lockedInstance.getFavoriteNames();
		for (auto i = l_fav_users.cbegin(); i != l_fav_users.cend(); ++i) // TODO - проверить на большом фаворите.
		{
			const string& l_nick =  *i;
			dcassert(!l_nick.empty());
			
			lSearchFrom = 0;
			const CAtlString sNick(WinUtil::toAtlString(l_nick));
			while (true)
			{
				lMyNickStart = sMsgLower.Find(sNick, lSearchFrom);
				if (lMyNickStart < 0)
					break; // TODO не понятно зачем грузить список ников из фаворитов чтобы при первом ошибочном поиске выйти из цикла!
				// [!] SSA - get Previous symbol.
				if (lMyNickStart > 0 && !isGoodNickBorderSymbol(sMsgLower.GetAt(lMyNickStart - 1)))
					break;
					
				lMyNickEnd = lMyNickStart + sNick.GetLength();
				
				// [!] SSA - get Last symbol.
				if (lMyNickEnd < sMsgLower.GetLength() - 1 && !isGoodNickBorderSymbol(sMsgLower.GetAt(lMyNickEnd)))
					break;
					
				SetSel(lSelBegin + lMyNickStart, lSelBegin + lMyNickEnd);
				
				auto tmpCF = Colors::g_TextStyleFavUsers;
				SetSelectionCharFormat(tmpCF);
				lSearchFrom = lMyNickEnd;
			}
		}
	}
}
void ChatCtrl::AppendTextParseURL(CAtlString& sMsgLower, const CFlyChatCacheTextOnly& p_message, const LONG& lSelBegin)
{
	// Zvyrazneni vsech URL a nastaveni "klikatelnosti"
	// const LONG lSelEnd = GetTextLengthEx(GTL_NUMCHARS);
	LONG lSearchFrom = 0;
	tstring ls;
	for (size_t i = 0; i < _countof(g_AllLinks); i++)
	{
		CAtlString currentLink(WinUtil::toAtlString(g_AllLinks[i].tag)); // TODO: rewrite without me!
		LONG linkStart = sMsgLower.Find(currentLink, lSearchFrom);
		while (linkStart != -1)
		{
			const LONG linkEndLine = sMsgLower.Find(_T('\n'), linkStart);
			// [+]IRainman
			LONG linkEndSpaceOrBad = linkEndLine;
			for (size_t j = 0; j < _countof(g_BadUrlSymbol); j++)
			{
				const LONG temp = sMsgLower.Find(g_BadUrlSymbol[j], linkStart);
				if (temp != -1 && temp < linkEndSpaceOrBad)
					linkEndSpaceOrBad = temp;
			}
			// [~]IRainman
			LONG linkEnd;
			if (linkEndSpaceOrBad != -1)
			{
				linkEnd = linkEndSpaceOrBad;
			}
			else
			{
				linkEnd = _tcslen(sMsgLower); //-V103
			}
			ls.resize(linkEnd - linkStart); //-V106
			SetSel(lSelBegin + linkStart, lSelBegin + linkEnd);
			GetTextRange(lSelBegin + linkStart, lSelBegin + linkEnd, &ls[0]); // TODO проверить результат?
			tstring originalLink = ls;
			tstring l_weblink;
			tstring l_webdesc;
			if (Util::isMagnetLink(ls)) // shorten magnet-links
			{
				ls = ls.erase(0, 8); // opt: delete "magnet:?" for fasts search.
				
				// Cruth for stupid magnet-link generators.
				// see http://en.wikipedia.org/wiki/Magnet_URI_scheme for details about paramethers.
				{
					static const tstring Paramethers[] =
					{
						_T("xl="), // (eXact Length) - Size in bytes
						_T("dn="), // (Display Name) - Filename
						_T("dl="), // (Display Lenght) - Display Lenght (unofficial paramethers!)
						_T("xt="), // (eXact Topic) - URN containing file hash
						_T("as="), // (Acceptable Source) - Web link to the file online
						_T("xs="), // (eXact Source) - P2P link.
						_T("kt="), // (Keyword Topic) - Key words for search
						_T("mt="), // (Manifest Topic) - link to the metafile that contains a list of magneto (MAGMA - MAGnet MAnifest)
						_T("tr="), // (address TRacker) - Tracker URL for BitTorrent downloads
						_T("x.video="), // (Run video on preview) [!] SSA
						_T("video="), // (Run video on preview) [!] SSA
						_T("x.do"), // (Description Online URL) - Web link to the Description online
					};
					tstring temp = originalLink;
					tstring::size_type i = 0;
					bool needsToReplaceOrig = false;
					while (1)
					{
						i = temp.find(_T('&'), i);
						if (i == tstring::npos)
							break;
							
						bool isInvalidParamFound = false;
						for (size_t count = 0; count < _countof(Paramethers); count++)
						{
							if (temp.compare(i + 1, Paramethers[count].size(), Paramethers[count]) == 0)
							{
								isInvalidParamFound = false;
								break;
							}
							else
								isInvalidParamFound = true;
						}
						
						if (isInvalidParamFound)
						{
							needsToReplaceOrig = true;
							temp.replace(i, 1, _T("%26"));
							i += 3;
						}
						else
							i++;
					};
					
					if (needsToReplaceOrig)
					{
						ls = temp;
						originalLink = temp;
					}
				}
				
				tstring sFileName = ls;
				const bool l_isTorrentLink = Util::isTorrentLink(sFileName);
				
				auto getParamether = [](const tstring & p_param, tstring & p_strInOut) -> bool
				{
					const auto l_start = p_strInOut.find(p_param);
					if (l_start != string::npos)
					{
						p_strInOut = p_strInOut.substr(l_start + p_param.size());
						
						const auto l_end = p_strInOut.find(_T('&'));
						if (l_end != string::npos)
							p_strInOut = p_strInOut.substr(0, l_end);
							
						return true;
					}
					return false;
				};
				
				if (getParamether(_T("dn="), sFileName))
					sFileName = Text::toT(Util::encodeURI(Text::fromT(sFileName), true));
				else
					sFileName = TSTRING(UNNAMED_MAGNET_LINK);
					
				tstring sDispWebLink = ls;
				tstring sDispWebDesc = ls;
				if (getParamether(_T("as="), sDispWebLink))
				{
					if (!sDispWebLink.empty() && (Util::isHttpLink(sDispWebLink) || Util::isHttpsLink(sDispWebLink)/* || Util::isFtpLink(sDispWebLink)*/))
						l_weblink = sDispWebLink;
				}
				if (getParamether(_T("x.do="), sDispWebDesc))
				{
					if (!sDispWebDesc.empty() && (Util::isHttpLink(sDispWebDesc) || Util::isHttpsLink(sDispWebDesc)))
						l_webdesc = sDispWebDesc;
				}
				
				if (l_isTorrentLink)
					ls = sFileName + _T(" (") + TSTRING(BT_LINK) + _T(')');
				else
				{
					tstring sFileSize = ls;
					int64_t filesize = -1;
					if (getParamether(_T("xl="), sFileSize))
						filesize = Util::toInt64(sFileSize);
						
					tstring sDispLine = ls;
					int64_t dlsize = -1;
					if (getParamether(_T("dl="), sDispLine))
						dlsize = Util::toInt64(sDispLine); // [+] Scalolaz get DCLST size (Display Length) if isset/required //-V106
						
					ls = sFileName;
					ls += _T(" (");
					if (filesize >= 0)
					{
						ls += Util::formatBytesW(filesize);
						if (dlsize >= 0)
							ls += _T(", ") + TSTRING(SETTINGS_SHARE_SIZE) + _T(' ') + Util::formatBytesW(dlsize); // [+] Scalolaz (DCLST)
					}
					else
						ls += TSTRING(INVALID_SIZE);
						
					if (getParamether(_T("xs="), sDispLine))
					{
						if (!sDispLine.empty())
							ls += _T(", ") + TSTRING(HUB) + _T(": ") + Text::toT(Util::formatDchubUrl(Text::fromT(sDispLine)));
					}
					ls += _T(')');
				}
			}
			/* TODO
			            else if (Util::isImageLink(ls))
			            {
			                // Try to download and insert image
			                string sLinkFullName;
			                Text::fromT(ls, sLinkFullName);
			                Text::toT(Util::encodeURI(sLinkFullName, true), ls);
			            }
			*/
			else
			{
				// [!] IRainman decode all URI!
				string sLinkFullName;
				Text::fromT(ls, sLinkFullName);
				Text::toT(Util::encodeURI(sLinkFullName, true), ls);
			}
			// Prepare string for buffering URL, URI...
			int l_lenght = 0;
			if (!l_weblink.empty())
			{
				tstring l_ls = _T("  ( ") + TSTRING(WEB_URL) + _T(":") + Text::toT(Util::encodeURI(Text::fromT(l_weblink), true)) + _T(" )");
				l_lenght += l_ls.length();
				ls += l_ls;
			}
			if (!l_webdesc.empty())
			{
				tstring l_lsd = _T("  ( ") + TSTRING(DESCRIPTION) + _T(":") + Text::toT(Util::encodeURI(Text::fromT(l_webdesc), true)) + _T(" )");
				l_lenght += l_lsd.length();
				ls += l_lsd;
			}
			ReplaceSel(ls.c_str());
			sMsgLower = sMsgLower.Left(linkStart) + WinUtil::toAtlString(ls) + sMsgLower.Mid(linkEnd);
			linkEnd = linkStart + ls.length() - l_lenght; //-V104 //-V103
			SetSel(lSelBegin + linkStart, lSelBegin + linkEnd);
			m_URLMap.push_back(TURLPair(lSelBegin + linkStart, originalLink));
			if (!l_weblink.empty())
			{
				linkEnd = linkEnd + l_lenght;
				//l_weblink.erase();
			}
			auto tmpCF = Colors::g_TextStyleURL;
			SetSelectionCharFormat(tmpCF);
			linkStart = sMsgLower.Find(currentLink, linkEnd);
		}
	}
	
}
void ChatCtrl::AppendTextParseBB(CAtlString& sMsgLower, const CFlyChatCacheTextOnly& p_message, const LONG& lSelBegin)
{
#ifdef IRAINMAN_USE_BB_CODES
	// BB codes support http://ru.wikipedia.org/wiki/BbCode
	if (BOOLSETTING(FORMAT_BB_CODES))
	{
		//const LONG lSelEnd = GetTextLengthEx(GTL_NUMCHARS);
		for (size_t i = 0; i < _countof(g_StartBBTag); i++)
		{
			const CAtlString currentTag(WinUtil::toAtlString(g_StartBBTag[i].tag)); // TODO: rewrite with out me!
			LONG BBStart = sMsgLower.Find(currentTag, 0);
			while (BBStart != -1)
			{
				LONG BBEnd;
				const CAtlString currentTagEnd(WinUtil::toAtlString(g_EndBBTag[i].tag));
				const LONG BBEndTerminate = sMsgLower.Find(currentTagEnd, BBStart + currentTag.GetLength());
				if (BBEndTerminate > BBStart)
				{
					BBEnd = BBEndTerminate;
				}
				else
				{
					BBStart = BBEnd = _tcslen(sMsgLower); //-V103
				}
				
				// No closing, no formatting...
				if (BBEnd == BBEndTerminate)
				{
					// TODO "code" tags
					//bool l_needsToFormat = true;
					//const long rtfEndIncludeTag = rtfEnd + g_EndBBTag[i].size;
					//for(CIterIntervals j = l_NonFormatIntervals.cbegin(); j != l_NonFormatIntervals.cend(); ++j)
					//{
					//  if ((rtfStart > j->start && rtfStart < j->end) ||
					//      (rtfEnd > j->start && rtfEndIncludeTag < j->end))
					//  {
					//      l_needsToFormat = false;
					//      break;
					//  }
					//}
					
					//if (l_needsToFormat) TODO
					//{
					CHARFORMAT2 temp = p_message.m_cf;
					
					AutoArray<TCHAR> l_TextToFormat(BBEnd - BBStart + 1);
					l_TextToFormat[0] = 0;
					
					size_t colorDelta = 0;
					
					//l_TextInCode.resize(BBEnd - BBStart + 1);
					const LONG l_StartIncludeTag = lSelBegin + BBStart;
					const LONG l_Start = l_StartIncludeTag + g_StartBBTag[i].tag.size(); //-V104 //-V103
					const LONG l_Stop = lSelBegin + BBEnd;
					const LONG l_StopIncludeTag = l_Stop + g_EndBBTag[i].tag.size(); //-V104 //-V103
					
					// Selection text include tag
					SetSel(l_StartIncludeTag, l_StopIncludeTag);
					
					if (l_Start != l_Stop) // No text to format
					{
						// GetText
						GetTextRange(l_Start, l_Stop, l_TextToFormat);
						tstring l_TextInCode = l_TextToFormat;
						//TODO optimize GetTextRange(l_Start, l_Stop, &l_TextInCode[0]);
						
						// TODO switch (i)
						{
							//case 0: TODO "code" tags
							//{
							//  l_NonFormatIntervals.push_back(Interval(rtfStart, rtfEnd));
							//
							//  // Replace text
							//  ReplaceSel(l_TextInCode.c_str());
							//}
							//break;
							// TODO default:
							do
							{
								// Set mask
								temp.dwMask = CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE | CFM_STRIKEOUT | /*TODO: CFM_HIDDEN |*/ CFM_SPACING;
								
								// Get previous formatting
								GetSelectionCharFormat(temp);
								
								temp.dwMask = CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE | CFM_STRIKEOUT | /*TODO: CFM_HIDDEN |*/ CFM_SPACING;
								
								// Set new format bit
								switch (i)
								{
									case 1:// Bold
										temp.dwEffects |= CFE_BOLD;
										break;
									case 2:// Italic
										temp.dwEffects |= CFE_ITALIC;
										break;
									case 3:// Underline
										temp.dwEffects |= CFE_UNDERLINE;
										break;
									case 4:// StrikeOut
										temp.dwEffects |= CFE_STRIKEOUT;
										break;
									case 5:// TODO Image support
										break;
									case 6: // Color support
										// We get #FF0000]Test
									{
										// get color item
										const auto endColorTagPosition = l_TextInCode.find(_T(']'));
										if (endColorTagPosition == tstring::npos)
											break;
											
										// get addition delta (size of item)
										colorDelta = endColorTagPosition + 1;
										// Check if we must use colors in text
										if (BOOLSETTING(FORMAT_BB_CODES_COLORS))
										{
											const tstring colorItem = l_TextInCode.substr(0, endColorTagPosition);
											if (Colors::getColorFromString(colorItem, temp.crTextColor/*, temp.crBackColor*/))
											{
												temp.dwMask |= CFM_COLOR;
												/*temp.dwMask |= CFM_BACKCOLOR;*/
											}
										}
										// get text
										l_TextInCode = l_TextInCode.substr(colorDelta);
									}
									break;
								}
								
								// Replace text
								ReplaceSel(l_TextInCode.c_str());
								
								// Select text to format
								SetSel(l_StartIncludeTag, l_StartIncludeTag + l_TextInCode.size()); //-V104
								
								// Set formatting to selection
								SetSelectionCharFormat(temp);
								
								// TODO скрывать а не удалять текст! это позволит в последствии копировать его без ошибок в буфер обмена
								// Set hidding bit http://www.cyberguru.ru/programming/visual-cpp/visual-cpp-std-classes-descr-page230.html
								//temp.dwMask = CFM_HIDDEN;
								//temp.dwEffects |= CFE_HIDDEN;
								
								// Hide end tags
								//SetSel(rtfEnd, g_EndBBTag[i].size);
								//SetSelectionCharFormat(temp);
								
								// Hide start tags
								//SetSel(rtfStart, g_StartBBTag[i].size);
								//SetSelectionCharFormat(temp);
							}
							while (false);  // [!] SSA - need this to exit this block
							// TODO break;
						}
					}
					else // Delete tags
						ReplaceSel(_T(""));
						
					// Clean end tags
					sMsgLower.Delete(BBEnd, g_EndBBTag[i].tag.size()); //-V107
					
					// Clean start tags
					sMsgLower.Delete(BBStart, g_StartBBTag[i].tag.size() + colorDelta); //-V107
				}
				BBStart = sMsgLower.Find(currentTag, BBStart);
			}
		}
	}
#endif // IRAINMAN_USE_BB_CODES
	
}
bool ChatCtrl::HitNick(const POINT& p, tstring& sNick, int& iBegin, int& iEnd, const UserPtr& user)
{
	LONG iCharPos = CharFromPos(p), line = LineFromChar(iCharPos), len = LineLength(iCharPos) + 1;
	LONG lSelBegin = 0, lSelEnd = 0;
	if (len < 3)
		return false;
		
	// Metoda FindWordBreak nestaci, protoze v nicku mohou byt znaky povazovane za konec slova
	int iFindBegin = LineIndex(line), iEnd1 = LineIndex(line) + LineLength(iCharPos);
	
	for (lSelBegin = iCharPos; lSelBegin >= iFindBegin; lSelBegin--)
	{
		if (FindWordBreak(WB_ISDELIMITER, lSelBegin))
			break;
	}
	lSelBegin++;
	for (lSelEnd = iCharPos; lSelEnd < iEnd1; lSelEnd++)
	{
		if (FindWordBreak(WB_ISDELIMITER, lSelEnd))
			break;
	}
	
	len = lSelEnd - lSelBegin;
	if (len <= 0)
		return false;
		
	tstring sText;
	sText.resize(static_cast<size_t>(len));
	
	GetTextRange(lSelBegin, lSelEnd, &sText[0]);
	
	size_t iLeft = 0, iRight = 0, iCRLF = sText.size(), iPos = sText.find(_T('<'));
	if (iPos != tstring::npos)
	{
		iLeft = iPos + 1;
		iPos = sText.find(_T('>'), iLeft);
		if (iPos == tstring::npos)
			return false;
			
		iRight = iPos - 1;
		iCRLF = iRight - iLeft + 1;
	}
	else
	{
		iLeft = 0;
	}
	
	tstring sN = sText.substr(iLeft, iCRLF);
	if (sN.empty())
		return false;
		
	if (user && user->getLastNick() == Text::fromT(sN)) // todo getIdentity().getNick()
	{
		if (user->isOnline())
		{
			sNick = sN;
			iBegin = lSelBegin + iLeft; //-V104 //-V103
			iEnd = lSelBegin + iLeft + iCRLF; //-V104 //-V103
			return true;
		}
		return false;
	}
	else
	{
		const Client* l_client = ClientManager::findClient(getHubHint());
		if (!l_client) // [+] IRainman opt.
			return false;
			
		if (isOnline(l_client, sN)) // [12] https://www.box.net/shared/1e2dd39bf1225b30d0f6
		{
			sNick = sN;
			iBegin = lSelBegin + iLeft; //-V104 //-V103
			iEnd = lSelBegin + iLeft + iCRLF; //-V104 //-V103
			return true;
		}
		
		// Jeste pokus odmazat eventualni koncovou ':' nebo '>'
		// Nebo pro obecnost posledni znak
		// A taky prvni znak
		// A pak prvni i posledni :-)
		if (iCRLF > 1)
		{
			sN = sText.substr(iLeft, iCRLF - 1);
			if (isOnline(l_client, sN))  // !SMT!-S
			{
				sNick = sN;
				iBegin = lSelBegin + iLeft; //-V104 //-V103
				iEnd = lSelBegin + iLeft + iCRLF - 1; //-V104 //-V103
				return true;
			}
			
			sN = sText.substr(iLeft + 1, iCRLF - 1);
			if (isOnline(l_client, sN))  // !SMT!-S
			{
				sNick = sN;
				iBegin = lSelBegin + iLeft + 1; //-V104 //-V103
				iEnd = lSelBegin + iLeft + iCRLF; //-V103 //-V104
				return true;
			}
			
			sN = sText.substr(iLeft + 1, iCRLF - 2);
			if (isOnline(l_client, sN))  // !SMT!-S
			{
				sNick = sN;
				iBegin = lSelBegin + iLeft + 1; //-V104 //-V103
				iEnd = lSelBegin + iLeft + iCRLF - 1; //-V104 //-V103
				return true;
			}
		}
		return false;
	}
}

bool ChatCtrl::HitIP(const POINT& p, tstring& sIP, int& iBegin, int& iEnd)
{
	// TODO: add IPv6 support.
	const int iCharPos = CharFromPos(p);
	int len = LineLength(iCharPos) + 1;
	if (len < 7) // 1.1.1.1
		return false;
		
	DWORD lPosBegin = FindWordBreak(WB_LEFT, iCharPos);
	DWORD lPosEnd = FindWordBreak(WB_RIGHTBREAK, iCharPos);
	len = lPosEnd - lPosBegin;
	
	if (len < 7) // 1.1.1.1
		return false;
		
	tstring sText;
	sText.resize(static_cast<size_t>(len));
	GetTextRange(lPosBegin, lPosEnd, &sText[0]);
	
	// TODO: сleanup.
	for (size_t i = 0; i < sText.size(); i++)
	{
		if (!((sText[i] == '\0') || (sText[i] == '.') || ((sText[i] >= '0') && (sText[i] <= '9'))))
		{
			return false;
		}
	}
	
	sText += _T('.');
	size_t iFindBegin = 0, iPos = tstring::npos, iEnd2 = 0;
	
	for (int i = 0; i < 4; ++i) //-V112
	{
		iPos = sText.find(_T('.'), iFindBegin);
		if (iPos == tstring::npos)
		{
			return false;
		}
		iEnd2 = atoi(Text::fromT(sText.substr(iFindBegin)).c_str());
		if (/*[-] PVS (iEnd2 < 0) || */ (iEnd2 > 255))  // [!] PVS V547  Expression 'iEnd2 < 0' is always false. Unsigned type value is never < 0.
		{
			return false;
		}
		iFindBegin = iPos + 1;
	}
	
	sIP = sText.substr(0, iPos);
	iBegin = lPosBegin;
	iEnd = lPosEnd;
	
	return true;
}

tstring ChatCtrl::LineFromPos(const POINT& p) const
{
	const int iCharPos = CharFromPos(p);
	const int len = LineLength(iCharPos);
	
	if (len < 3)
	{
		return Util::emptyStringT;
	}
	
	tstring tmp;
	tmp.resize(static_cast<size_t>(len));
	
	GetLine(LineFromChar(iCharPos), &tmp[0], len);
	
	return tmp;
}
void ChatCtrl::GoToEnd(POINT& p_scroll_pos, bool p_force)
{
	SCROLLINFO si = { 0 };
	si.cbSize = sizeof(si);
	si.fMask = SIF_PAGE | SIF_RANGE | SIF_POS;
	GetScrollInfo(SB_VERT, &si);
	if (m_boAutoScroll || p_force)
	{
		// this must be called twice to work properly :(
		PostMessage(EM_SCROLL, SB_BOTTOM, 0);
		PostMessage(EM_SCROLL, SB_BOTTOM, 0);
	}
	SetScrollPos(&p_scroll_pos);
}
void ChatCtrl::GoToEnd(bool p_force)
{
	POINT pt = { 0 };
	GetScrollPos(&pt);
	GoToEnd(pt, p_force);
	if (m_boAutoScroll || p_force)
	{
		// this must be called twice to work properly :(
		PostMessage(EM_SCROLL, SB_BOTTOM, 0);
		PostMessage(EM_SCROLL, SB_BOTTOM, 0);
	}
}

void ChatCtrl::invertAutoScroll()
{
	SetAutoScroll(!m_boAutoScroll);
}

void ChatCtrl::SetAutoScroll(bool boAutoScroll)
{
	m_boAutoScroll = boAutoScroll;
	if (m_boAutoScroll)
	{
		GoToEnd(false);
	}
}

LRESULT ChatCtrl::OnRButtonDown(POINT pt, const UserPtr& user /*= nullptr*/)
{
	g_sSelectedLine = LineFromPos(pt);
	g_sSelectedText.clear();
	g_sSelectedUserName.clear();
	g_sSelectedIP.clear();
	g_sSelectedURL.clear();
	
	// Po kliku dovnitr oznaceneho textu si zkusime poznamenat pripadnej nick ci ip...
	// jinak by nam to neuznalo napriklad druhej klik na uz oznaceny nick =)
	long lSelBegin, lSelEnd;
	GetSel(lSelBegin, lSelEnd);
	
	if (HitURL(g_sSelectedURL, lSelBegin/*, lSelEnd*/))
		return 1;
		
	const int iCharPos = CharFromPos(pt);
	int iBegin, iEnd;
	if ((lSelEnd > lSelBegin) && (iCharPos >= lSelBegin) && (iCharPos <= lSelEnd))
	{
		if (!HitIP(pt, g_sSelectedIP, iBegin, iEnd))
			if (!HitNick(pt, g_sSelectedUserName, iBegin, iEnd, user))
				HitText(g_sSelectedText, lSelBegin, lSelEnd);
				
		return 1;
	}
	
	// hightlight IP or nick when clicking on it
	if (HitIP(pt, g_sSelectedIP, iBegin, iEnd) || HitNick(pt, g_sSelectedUserName, iBegin, iEnd, user))
		// [8] https://www.box.net/shared/132998a58df880723a78
	{
		SetSel(iBegin, iEnd);
		InvalidateRect(nullptr);
	}
	return 1;
}

//[+] sergiy.karasov
//отключение автоскролла в окне чата при вращении колеса мыши вверх
//влючать автоскролл либо в меню (по правому клику), либо проскроллировав (колесом мыши) до самого низа
LRESULT ChatCtrl::onMouseWheel(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	RECT rc;
	POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }; // location of mouse click
	SCROLLINFO si = { 0 };
	
	// Get the bounding rectangle of the client area.
	ChatCtrl::GetClientRect(&rc);
	ChatCtrl::ScreenToClient(&pt);
	if (PtInRect(&rc, pt))
	{
		si.cbSize = sizeof(si);
		si.fMask = SIF_PAGE | SIF_RANGE | SIF_POS;
		GetScrollInfo(SB_VERT, &si);
		
		if (GET_WHEEL_DELTA_WPARAM(wParam) > 0) //positive - wheel was rotated forward
		{
			if (si.nMin != si.nPos) //бегунок не в начале или есть скроллбар
			{
				SetAutoScroll(false);
			}
		}
		else //negative value indicates that the wheel was rotated backward, toward the user
		{
			//макс достигнут
			//проблемка - мы обрабатываем WM_MOUSEWHEEL перед его основным обработчиком,
			//поэтому автоскролл, зачастую, врубается при дополнительном (т.е. конец уже достигнут, но надо еще раз)
			//вращении колеса мышки вниз
			if (si.nPos == int(si.nMax - si.nPage))
			{
				SetAutoScroll(true);
			}
		}
	}
	bHandled = FALSE;
	return 1;
}

//[+] sergiy.karasov
//фикс управления прокруткой окна чата при изменении размеров окна передач
LRESULT ChatCtrl::onSize(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (wParam != SIZE_MINIMIZED && HIWORD(lParam) > 0)
		if (IsWindow())
		{
			CHARRANGE l_cr;
			GetSel(l_cr);
			// [!] IRainman fix: 0 is valid value! Details http://msdn.microsoft.com/ru-ru/library/1z3s90k4(v=vs.90).aspx
			// [-] if (l_cr.cpMax > 0 && l_cr.cpMin > 0) //[+]PPA
			// [~]
			{
				SetSel(GetTextLengthEx(GTL_NUMCHARS), -1);
				ScrollCaret(); // [1] https://www.box.net/shared/qve5a2y5gcg2sopjbpd5
				const DWORD l_go = GetOptions();
				SetOptions(ECOOP_AND, DWORD(~(ECO_AUTOVSCROLL | ECO_AUTOHSCROLL)));
				SetSel(l_cr);
				SetOptions(ECOOP_OR, l_go);
				PostMessage(EM_SCROLL, SB_BOTTOM, 0);
			}
		}
		
	bHandled = FALSE;
	return 1;
}
const tstring& ChatCtrl::get_URL(ENLINK* p_EL) const
{
	return get_URL(p_EL->chrg.cpMin/*, p_EL->chrg.cpMax*/);
}
const tstring& ChatCtrl::get_URL(const long lBegin/*, const long lEnd*/) const
{
	for (auto i = m_URLMap.cbegin(); i != m_URLMap.cend(); ++i)
	{
		if (i->first == lBegin)
			return i->second;
	}
	//dcassert(0);
	return Util::emptyStringT;
}
tstring ChatCtrl::get_URL_RichEdit(ENLINK* p_EL) const
{
	LONG utlBeg = p_EL->chrg.cpMin;
	LONG utlEnd = p_EL->chrg.cpMax;
	tstring l_url;
	if (utlEnd - utlBeg > 0)
	{
		const HWND hRichEdit = p_EL->nmhdr.hwndFrom;
		l_url.resize(utlEnd - utlBeg + 1);
		SendMessageW(hRichEdit, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&p_EL->chrg));
		SendMessageW(hRichEdit, EM_GETSELTEXT, 0, reinterpret_cast<LPARAM>(l_url.data()));
		SendMessageW(hRichEdit, EM_SETSEL, utlEnd, utlEnd);
	}
	return l_url;
}
LRESULT ChatCtrl::onEnLink(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	ENLINK* pEL = (ENLINK*)pnmh;
	if (pEL->msg == WM_LBUTTONUP || pEL->msg == WM_RBUTTONUP)
	{
		if (pEL->msg == WM_LBUTTONUP)
		{
			g_sSelectedURL = get_URL(pEL);
			dcassert(!g_sSelectedURL.empty());
			if (g_sSelectedURL.empty())
			{
				g_sSelectedURL = get_URL_RichEdit(pEL);
				LogManager::message("Error get URL from position = " + Util::toString(pEL->chrg.cpMin) + " Use RichEditURL = " + Text::fromT(g_sSelectedURL));
			}
			WinUtil::openLink(g_sSelectedURL);
		}
		else if (pEL->msg == WM_RBUTTONUP)
		{
			g_sSelectedURL = get_URL(pEL);
			if (g_sSelectedURL.empty())
			{
				g_sSelectedURL = get_URL_RichEdit(pEL);
				LogManager::message("Error get URL from position = " + Util::toString(pEL->chrg.cpMin) + " Use RichEditURL = " + Text::fromT(g_sSelectedURL));
			}
			SetSel(pEL->chrg.cpMin, pEL->chrg.cpMax);
			InvalidateRect(NULL);
			return 0;
		}
	}
	return 0;
}

LRESULT ChatCtrl::onCopyActualLine(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!g_sSelectedLine.empty())
	{
		WinUtil::setClipboard(g_sSelectedLine);
	}
	return 0;
}

LRESULT ChatCtrl::onCopyURL(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!g_sSelectedURL.empty())
	{
		WinUtil::setClipboard(g_sSelectedURL);
	}
	return 0;
}
#ifdef IRAINMAN_ENABLE_WHOIS
LRESULT ChatCtrl::onWhoisIP(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!g_sSelectedIP.empty())
	{
		WinUtil::CheckOnWhoisIP(wID, g_sSelectedIP);
	}
	return 0;
}
LRESULT ChatCtrl::onWhoisURL(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!g_sSelectedURL.empty())
	{
		uint16_t port;
		string proto, host, file, query, fragment;
		Util::decodeUrl(Text::fromT(g_sSelectedURL), proto, host, port, file, query, fragment);
		if (!host.empty())
			WinUtil::openLink(_T("http://bgp.he.net/dns/") + Text::toT(host) + _T("#_website"));
	}
	return 0;
}
#endif
LRESULT ChatCtrl::onEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	Copy();
	return 0;
}

LRESULT ChatCtrl::onEditSelectAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	SetSelAll();
	return 0;
}

LRESULT ChatCtrl::onEditClearAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	Clear(); // [!] IRainman fix.
	return 0;
}
// !SMT!-S
bool ChatCtrl::isOnline(const Client* client, const tstring& aNick)
{
	return client->findUser(Text::fromT(aNick)) != nullptr;// [!] IRainman opt.
}

bool ChatCtrl::isGoodNickBorderSymbol(const TCHAR ch)
{
	for (size_t j = 0; j < _countof(g_GoodBorderNickSymbol); j++)
	{
		if (ch == g_GoodBorderNickSymbol[j])
		{
			return true;
		}
	}
	return false;
}

#ifdef IRAINMAN_INCLUDE_SMILE
// TODO - никогда не зовется
LRESULT ChatCtrl::onUpdateSmile(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	IOleClientSite *spClientSite = (IOleClientSite *)lParam;
	
	if (spClientSite)
	{
		CComPtr<IOleInPlaceSite> spInPlaceSite;
		spClientSite->QueryInterface(__uuidof(IOleInPlaceSite), (void **)&spInPlaceSite);
		
		if (spInPlaceSite)// && spInPlaceSite->GetWindow(&hwndParent) == S_OK)
		{
			OLEINPLACEFRAMEINFO frameInfo;
			RECT rcPos, rcClip;
			frameInfo.cb = sizeof(OLEINPLACEFRAMEINFO);
			
			if (spInPlaceSite->GetWindowContext(NULL,
			                                    NULL, &rcPos, &rcClip, NULL) == S_OK)
			{
				::InvalidateRect(m_hWnd, &rcPos, FALSE);
			}
		}
		safe_release(spClientSite);
	}
	
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ChatCtrl::QueryInterface(THIS_ REFIID riid, LPVOID FAR * lplpObj)
{
	HRESULT res = E_NOINTERFACE;
	
	if (riid == IID_IRichEditOleCallback)
	{
		*lplpObj = this;
		res = S_OK;
	}
	
	return res;
}

COM_DECLSPEC_NOTHROW ULONG STDMETHODCALLTYPE ChatCtrl::AddRef(THIS)
{
	return InterlockedIncrement(&m_Ref);
}

COM_DECLSPEC_NOTHROW ULONG STDMETHODCALLTYPE ChatCtrl::Release(THIS)
{
	return InterlockedDecrement(&m_Ref);
}

// *** IRichEditOleCallback methods ***
COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE ChatCtrl::GetNewStorage(THIS_ LPSTORAGE FAR * lplpstg)
{
	return S_OK;
}

COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE ChatCtrl::GetInPlaceContext(THIS_ LPOLEINPLACEFRAME FAR * lplpFrame,
                                                                           LPOLEINPLACEUIWINDOW FAR * lplpDoc,
                                                                           LPOLEINPLACEFRAMEINFO lpFrameInfo)
{
	if (lplpFrame)
		*lplpFrame = nullptr;
	if (lplpDoc)
		*lplpDoc = nullptr;
	return S_OK;
}

COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE ChatCtrl::ShowContainerUI(THIS_ BOOL fShow)
{
	return S_OK;
}

COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE ChatCtrl::QueryInsertObject(THIS_ LPCLSID lpclsid, LPSTORAGE lpstg,
                                                                           LONG cp)
{
	return S_OK;
}

COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE ChatCtrl::DeleteObject(THIS_ LPOLEOBJECT lpoleobj)
{
	IGDIImageDeleteNotify *pDeleteNotify;
	if (lpoleobj->QueryInterface(IID_IGDIImageDeleteNotify, (void**)&pDeleteNotify) == S_OK && pDeleteNotify)
	{
		pDeleteNotify->SetDelete();
		safe_release(pDeleteNotify);
	}
	
	return S_OK;
}

COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE ChatCtrl::QueryAcceptData(THIS_ LPDATAOBJECT lpdataobj,
                                                                         CLIPFORMAT FAR * lpcfFormat, DWORD reco,
                                                                         BOOL fReally, HGLOBAL hMetaPict)
{
	return S_OK;
}

COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE ChatCtrl::ContextSensitiveHelp(THIS_ BOOL fEnterMode)
{
	return S_OK;
}

COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE ChatCtrl::GetClipboardData(THIS_ CHARRANGE FAR * lpchrg, DWORD reco,
                                                                          LPDATAOBJECT FAR * lplpdataobj)
{
	return E_NOTIMPL;
}
COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE ChatCtrl::GetDragDropEffect(THIS_ BOOL fDrag, DWORD grfKeyState,
                                                                           LPDWORD pdwEffect)
{
	return S_OK;
}
COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE ChatCtrl::GetContextMenu(THIS_ WORD seltype, LPOLEOBJECT lpoleobj,
                                                                        CHARRANGE FAR * lpchrg,
                                                                        HMENU FAR * lphmenu)
{
	// Forward message to host window
	::DefWindowProc(m_hWnd, WM_CONTEXTMENU, (WPARAM)m_hWnd, GetMessagePos());
	return S_OK;
}
void ChatCtrl::setHubParam(const string& sUrl, const string& sNick)
{
	if (!sNick.empty())
	{
		m_MyNickLower = WinUtil::toAtlString(sNick);
		m_MyNickLower.MakeLower();
	}
	else
	{
		m_MyNickLower = _T("");
	}
	m_HubHint = sUrl;    // !SMT!-S
}

#endif // IRAINMAN_INCLUDE_SMILE