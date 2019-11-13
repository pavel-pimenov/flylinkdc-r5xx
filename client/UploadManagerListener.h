
#pragma once

#ifndef DCPLUSPLUS_DCPP_UPLOADMANAGERLISTENER_H_
#define DCPLUSPLUS_DCPP_UPLOADMANAGERLISTENER_H_

#include "typedefs.h"
#include "Upload.h"

class UploadManagerListener
{
		friend class UploadQueueItem;
	public:
		virtual ~UploadManagerListener() { }
		template<int I> struct X
		{
			enum { TYPE = I };
		};
		
		typedef X<0> Complete;
		typedef X<1> Failed;
		typedef X<2> Starting;
		typedef X<3> Tick;
		typedef X<4> QueueAdd;
		typedef X<5> QueueRemove;
		typedef X<6> QueueItemRemove;
		typedef X<7> QueueUpdate;
		
		virtual void on(Starting, const UploadPtr& aUpload) noexcept { }
		virtual void on(Tick, const UploadArray&) noexcept {}
		virtual void on(Complete, const UploadPtr& aUpload) noexcept { }
		virtual void on(Failed, const UploadPtr& aUpload, const string&) noexcept { }
		virtual void on(QueueAdd, const UploadQueueItemPtr&) noexcept { }
		virtual void on(QueueRemove, const UserPtr&) noexcept { }
		virtual void on(QueueItemRemove, const UploadQueueItemPtr&) noexcept { }
		virtual void on(QueueUpdate) noexcept { }
		
};

#endif /*UPLOADMANAGERLISTENER_H_*/
