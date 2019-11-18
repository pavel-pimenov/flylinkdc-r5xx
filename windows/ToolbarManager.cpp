////////////////////////////////////////////////
//  ToolbarManager.cpp
//
//  This is based on portions of:
//  http://www.codeproject.com/wtl/regst.asp
//  (Copyright (c) 2001 Magomed Abdurakhmanov)
//
//  Changed save to xml file instead of registry
//
//  No warranties given. Provided as is.

#include "stdafx.h"
#include "ToolbarManager.h"
#include "../client/ClientManager.h"

ToolbarEntry::List ToolbarManager::g_toolbarEntries;
CriticalSection ToolbarManager::g_cs;

ToolbarManager::ToolbarManager()
{
}

ToolbarManager::~ToolbarManager()
{
	dcassert(0);
}

void ToolbarManager::shutdown()
{
	CFlyLock(g_cs);
	for_each(g_toolbarEntries.begin(), g_toolbarEntries.end(), DeleteFunction());
	g_toolbarEntries.clear();
}

void ToolbarManager::load(SimpleXML& aXml)
{
	aXml.resetCurrentChild();
	if (aXml.findChild("Rebars"))
	{
		aXml.stepIn();
		CFlyLock(g_cs);
		while (aXml.findChild("Rebar"))
		{
			ToolbarEntry* t = new ToolbarEntry();
			t->setName(aXml.getChildAttrib("Name"));
			t->setID(aXml.getChildAttrib("ID"));
			t->setCX(aXml.getChildAttrib("CX"));
			t->setBreakLine(aXml.getChildAttrib("BreakLine"));
			t->setBandCount(aXml.getIntChildAttrib("BandCount"));
			addToolBarEntryL(t);
		}
		aXml.stepOut();
	}
	else
	{
		// Default toolbar layout - for MainFrame rebar
		ToolbarEntry* t = new ToolbarEntry();
		t->setName("MainToolBar");
		t->setID("60160,60162,60161,60163");
		t->setCX("1147,213,1142,255");
		t->setBreakLine("1,0,1,1");
		t->setBandCount(4);
		{
			CFlyLock(g_cs);
			addToolBarEntryL(t);
		}
	}
}

void ToolbarManager::save(SimpleXML& aXml)
{
	aXml.addTag("Rebars");
	aXml.stepIn();
	CFlyLock(g_cs);
	for (auto i = g_toolbarEntries.cbegin(); i != g_toolbarEntries.cend(); ++i)
	{
		aXml.addTag("Rebar");
		aXml.addChildAttrib("Name", (*i)->getName());
		aXml.addChildAttrib("ID", (*i)->getID());
		aXml.addChildAttrib("CX", (*i)->getCX());
		aXml.addChildAttrib("BreakLine", (*i)->getBreakLine());
		aXml.addChildAttrib("BandCount", (*i)->getBandCount());
	}
	
	aXml.stepOut();
}

void ToolbarManager::getFrom(CReBarCtrl& ReBar, const string& aName)
{
	dcassert(ReBar.IsWindow());
	if (ReBar.IsWindow())
	{
		CFlyLock(g_cs);
		removeToolbarEntryL(getToolbarEntryL(aName));
		
		ToolbarEntry* t = new ToolbarEntry();
		string id, cx, bl, dl;
		t->setName(aName);
		t->setBandCount(ReBar.GetBandCount());
		
		for (int i = 0; i < t->getBandCount(); i++)
		{
			dl = i > 0 ? "," : "";
			REBARBANDINFO rbi = { 0 };
			rbi.cbSize = sizeof(rbi);
			rbi.fMask = RBBIM_ID | RBBIM_SIZE | RBBIM_STYLE;
			ReBar.GetBandInfo(i, &rbi);
			id += dl + Util::toString(rbi.wID);
			cx += dl + Util::toString(rbi.cx);
			bl += dl + (((rbi.fStyle & RBBS_BREAK) != 0) ? "1" : "0");
			
		}
		
		t->setID(id);
		t->setCX(cx);
		t->setBreakLine(bl);
		addToolBarEntryL(t);
	}
}

void ToolbarManager::applyTo(CReBarCtrl& ReBar, const string& aName)
{
	dcassert(ReBar.IsWindow());
	if (ReBar.IsWindow())
	{
		CFlyLock(g_cs);
		const ToolbarEntry* t = getToolbarEntryL(aName);
		if (t)
		{
			const StringTokenizer<string> id(t->getID(), ',');
			const StringList& idList = id.getTokens();
			const StringTokenizer<string> cx(t->getCX(), ',');
			const StringList& cxList = cx.getTokens();
			const StringTokenizer<string> bl(t->getBreakLine(), ',');
			const StringList& blList = bl.getTokens();
			
			for (int i = 0; i < t->getBandCount(); i++)
			{
				ReBar.MoveBand(ReBar.IdToIndex(Util::toInt(idList[i])), i);
				REBARBANDINFO rbi = { 0 };
				rbi.cbSize = sizeof(rbi);
				rbi.fMask = RBBIM_ID | RBBIM_SIZE | RBBIM_STYLE;
				ReBar.GetBandInfo(i, &rbi);
				
				rbi.cx = Util::toInt(cxList[i]);
				if (Util::toInt(blList[i]) > 0)
					rbi.fStyle |= RBBS_BREAK;
				else
					rbi.fStyle &= (~RBBS_BREAK);
					
				ReBar.SetBandInfo(i, &rbi);
			}
		}
	}
}

void ToolbarManager::removeToolbarEntryL(const ToolbarEntry* entry)
{
	if (entry)
	{
		auto i = find(g_toolbarEntries.begin(), g_toolbarEntries.end(), entry);
		if (i == g_toolbarEntries.end())
		{
			return;
		}
		g_toolbarEntries.erase(i);
		delete entry;
	}
}

const ToolbarEntry* ToolbarManager::getToolbarEntryL(const string& aName)
{
	for (auto i = g_toolbarEntries.cbegin(); i != g_toolbarEntries.cend(); ++i)
	{
		const ToolbarEntry* t = *i;
		if (stricmp(t->getName(), aName) == 0)
		{
			return t;
		}
	}
	return nullptr;
}