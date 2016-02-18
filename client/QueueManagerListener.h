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

#pragma once


#ifndef DCPLUSPLUS_DCPP_QUEUE_MANAGER_LISTENER_H
#define DCPLUSPLUS_DCPP_QUEUE_MANAGER_LISTENER_H

#include "forward.h"
#include "noexcept.h"
#include "Download.h"

class QueueManagerListener
{
	public:
		virtual ~QueueManagerListener() { }
		template<int I> struct X
		{
			enum { TYPE = I };
		};
		
		typedef X<0> Added;
		typedef X<1> Finished;
		typedef X<2> Removed;
		typedef X<3> Moved;
		typedef X<4> SourcesUpdated;
		typedef X<5> StatusUpdated;
		typedef X<6> PartialList;
		typedef X<8> RecheckStarted;
		typedef X<9> RecheckNoFile;
		typedef X<10> RecheckFileTooSmall;
		typedef X<11> RecheckDownloadsRunning;
		typedef X<12> RecheckNoTree;
		typedef X<13> RecheckAlreadyFinished;
		typedef X<14> RecheckDone;
		typedef X<15> AddedArray;
		typedef X<16> FileMoved;
		typedef X<17> TryAdding;
#ifdef SSA_VIDEO_PREVIEW_FEATURE
		typedef X<18> TryFileMoving;
#endif
		typedef X<19> Tick;
		typedef X<20> StatusUpdatedList;
		typedef X<21> RemovedArray;
		
		virtual void on(Added, const QueueItemPtr&) noexcept { }
		virtual void on(AddedArray, const std::vector<QueueItemPtr>& p_qi_array) noexcept { }
		virtual void on(Finished, const QueueItemPtr&, const string&, const DownloadPtr& p_download) noexcept { }
		virtual void on(Removed, const QueueItemPtr&) noexcept { }
		virtual void on(RemovedArray, const std::vector<string>& p_qi_array) noexcept { }
		virtual void on(Moved, const QueueItemPtr&, const string&) noexcept { }
		virtual void on(SourcesUpdated, const QueueItemPtr&) noexcept { }
		virtual void on(StatusUpdated, const QueueItemPtr&) noexcept { }
		virtual void on(Tick, const QueueItemList&) noexcept { } // [+] IRainman opt.
		virtual void on(StatusUpdatedList, const QueueItemList&) noexcept { } // [+] IRainman opt.
		virtual void on(PartialList, const HintedUser&, const string&) noexcept { }
		
		virtual void on(RecheckStarted, const string&) noexcept { }
		virtual void on(RecheckNoFile, const string&) noexcept { }
		virtual void on(RecheckFileTooSmall, const string&) noexcept { }
		virtual void on(RecheckDownloadsRunning, const string&) noexcept { }
		virtual void on(RecheckNoTree, const string&) noexcept { }
		virtual void on(RecheckAlreadyFinished, const string&) noexcept { }
		virtual void on(RecheckDone, const string&) noexcept { }
		
		virtual void on(FileMoved, const string&) noexcept { }
		virtual void on(TryAdding, const string& fileName, int64_t newSize, int64_t existingSize, time_t existingTime, int option) noexcept  { }
#ifdef SSA_VIDEO_PREVIEW_FEATURE
		virtual void on(TryFileMoving, const string&) noexcept { }
#endif
};

#endif // !defined(QUEUE_MANAGER_LISTENER_H)

/**
 * @file
 * $Id: QueueManagerListener.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
