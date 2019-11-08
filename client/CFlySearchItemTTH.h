//-----------------------------------------------------------------------------
//(c) 2017-2019 pavel.pimenov@gmail.com
//-----------------------------------------------------------------------------
#pragma once

#ifndef CFlySearchItemTTH_H
#define CFlySearchItemTTH_H

#include "HashValue.h"
#include "SearchQueue.h"


class CFlySearchItemTTH
#ifdef _DEBUG
	: boost::noncopyable
#endif
{
	public:
		TTHValue m_tth;
		std::string m_search;
		std::unique_ptr<std::string> m_toSRCommand;
		bool m_is_passive;
		bool m_is_skip;
		CFlySearchItemTTH(const TTHValue& p_tth, const std::string& p_search):
			m_is_passive(p_search.size() > 4 && p_search.compare(0, 4, "Hub:", 4) == 0),
			m_tth(std::move(p_tth)),
			m_search(std::move(p_search)),
			m_is_skip(false)
		{
			dcassert(m_search.size() > 4);
		}
		CFlySearchItemTTH(CFlySearchItemTTH && arg) :
			m_tth(std::move(arg.m_tth)),
			m_search(std::move(arg.m_search)),
			m_is_passive(std::move(arg.m_is_passive)),
			m_is_skip(std::move(arg.m_is_skip)),
			m_toSRCommand(std::move(arg.m_toSRCommand))
		{
		}
	private:
		CFlySearchItemTTH& operator=(CFlySearchItemTTH && other);
		//{
		//  member = std::move(other.member);
		//  return *this;
		//}
};
#if _MSC_VER > 1600 // > VC++2010
typedef std::vector<CFlySearchItemTTH> CFlySearchArrayTTH;
#else
typedef std::list<CFlySearchItemTTH> CFlySearchArrayTTH;
#endif

class CFlySearchItemFile : public SearchParam
{
};

typedef std::vector<CFlySearchItemFile> CFlySearchArrayFile;

#endif // CFlySearchItemTTH_H
