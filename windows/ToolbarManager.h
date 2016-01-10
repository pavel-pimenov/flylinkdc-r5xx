////////////////////////////////////////////////
//  ToolbarManager.h
//
//  This is based on portions of:
//  http://www.codeproject.com/wtl/regst.asp
//  (Copyright (c) 2001 Magomed Abdurakhmanov)
//
//  Changed save to xml file instead of registry
//
//  No warranties given. Provided as is.

#ifndef TOOLBARMANAGER_H
#define TOOLBARMANAGER_H

#pragma once

#include <atlbase.h>
#include <atlapp.h>
#include <atlctrls.h>

#include "../client/DCPlusPlus.h"

#include "../client/Singleton.h"
#include "../client/SettingsManager.h"
#include "../client/SimpleXML.h"

class ToolbarEntry
{
	public:
		typedef ToolbarEntry* Ptr;
		typedef vector<Ptr> List;
		
		ToolbarEntry() 
		{
			bandcount = 0;
		}
		~ToolbarEntry() { }
		
		GETSET(string, name, Name);
		GETSET(string, id, ID);
		GETSET(string, cx, CX);
		GETSET(string, breakline, BreakLine);
		GETSET(int, bandcount, BandCount);
};

class ToolbarManager: public Singleton<ToolbarManager>
{
	public:
		ToolbarManager();
		~ToolbarManager();
		
		// Get & Set toolbar positions
		void getFrom(CReBarCtrl& ReBar, const string& aName);
		void applyTo(CReBarCtrl& ReBar, const string& aName) const;

		// Save & load
		static void load(SimpleXML& aXml);
		static void save(SimpleXML& aXml);

	private:
		// Get data by name
		static ToolbarEntry* getToolbarEntry(const string& aName);
		
		// Remove old entry, when adding new
		static void removeToolbarEntry(const ToolbarEntry* entry);
		
		
		// Store Toolbar infos here
		static ToolbarEntry::List g_toolbarEntries;
		
};

#endif // TOOLBARMANAGER_H
