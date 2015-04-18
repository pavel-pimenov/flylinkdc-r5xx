/*
 * Copyright (C) 2011-2015 FlylinkDC++ Team http://flylinkdc.blogspot.com/
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

#if !defined(_VIDEO_PREVIEW_H_)
#define _VIDEO_PREVIEW_H_

#pragma once

#include "../client/Singleton.h"

#ifdef SSA_VIDEO_PREVIEW_FEATURE

#include "../client/Semaphore.h"
#include "../client/SettingsManager.h"
#include "../client/Socket.h"
#include "../client/BufferedSocket.h"
#include "../client/QueueItem.h"
#include "../client/QueueManagerListener.h"
#include "../FlyFeatures/FileRoadMap.h"
#include <map>
#include <queue>

#define WM_PREVIEW_SERVER_READY WM_USER+0x18
#define WM_PREVIEW_SERVER_LOG WM_USER+0x19

class VideoPreviewListener
{
	public:
		virtual ~VideoPreviewListener() { }
		template<int I> struct X
		{
			enum { TYPE = I };
		};
		
		typedef X<0> PreviewStarted;
		typedef X<1> Failed;
		
		virtual void on(PreviewStarted, const string&) noexcept { }
		virtual void on(Failed, const string&) noexcept { }
};

typedef std::map<string, string> HeaderMap;
typedef HeaderMap::iterator HeaderMapIter;

class VideoPreviewSocketProcessor: public Thread
{
	public:
	
		VideoPreviewSocketProcessor() : inProcess(false), isServerDie(false), sock(NULL), thread(NULL) {}
		
		void accept(const Socket& socket)
		{
			inProcess = true;
			int fromlen = sizeof(from);
			sock = ::accept(socket.m_sock, (struct sockaddr*) & from, &fromlen);
			u_long b = 1;
			ioctlsocket(sock, FIONBIO, &b);
		}
		int run();
		
		bool IsInProcess() const
		{
			return inProcess;
		}
		void setServerIsDie()
		{
			isServerDie = true;
		}
		
	private:
		void parseHeader(const string& headerString);
		
		
	private:
		sockaddr_in from;
		SOCKET sock;
		HANDLE thread;
		HeaderMap headers;
		volatile bool inProcess; // [!] IRainman fix: this variable is volatile.
		volatile bool isServerDie; // [!] IRainman fix: this variable is volatile.
};

typedef std::deque<VideoPreviewSocketProcessor*> SocketProcessorS; // [!] IRainman opt: change list to deque.
typedef SocketProcessorS::iterator SocketProcessorSIter;

typedef std::queue<std::string> LogInfoDataStack;

class VideoPreview :
	public Singleton<VideoPreview>, public Speaker<VideoPreviewListener>, public Thread, private SettingsManagerListener,
	public QueueItemDelegate, private QueueManagerListener
{
	public:
	
		void initialize();
		void AddExistingFileToPreview(const string& previewFile, const int64_t& previewFileSize, HWND serverReadyReport);
		void AddTempFileToPreview(QueueItem* tempItem, HWND serverReadyReport);
		
		void shutdown();
		
		bool IsServerStarted() const
		{
			return _serverStarted;
		}
		void StopServer();
		void StartServer();
		
		bool IsAcceptedConnection() const
		{
			return _isAcceptConnection;
		}
		const string& GetFilePreviewName() const
		{
			return _currentFilePreview;
		}
		bool IsAvailableData(int64_t pos, int64_t size) const;
		void setFileAlreadyDownloaded();
		int64_t GetFilePreviewSize() const
		{
			return _previewFileSize;
		}
		const string& GetFilePreviewTempName() const
		{
			return m_tempFilename;
		}
		void SetDownloadSegment(int64_t pos, int64_t size);
		bool IsPreviewFileExists() const
		{
			return !_currentFilePreview.compare(m_tempFilename);
		}
		bool CanUseFile() const
		{
			return _canUseFile;
		}
		void AddLogInfo(const string& loginfo);
		void SetLogDlgWnd(HWND window)
		{
			_logWnd = window;
		}
		bool GetNextLogItem(string& outString);
		
		
		// From QueueItemDelegate
		virtual void addSegment(int64_t start, int64_t size);
		virtual size_t getDownloadItems(int64_t blockSize, vector<int64_t>& ItemsArray);
		virtual void setDownloadItem(int64_t pos, int64_t size);
	private:
		friend class Singleton<VideoPreview>;
		bool failed;
		void clear();
		
		VideoPreview(): failed(false), _serverStarted(false), _serverPreview(false), _isServerDie(false), _isAcceptConnection(false)
			, _previewFileSize(0)
			, _canUseFile(true)
			, _viewStarted(true)			
		{
			_ask2Download.reserve(10);
			start(64, "VideoPreview");
		}
		
		virtual ~VideoPreview();
		
		enum Tasks
		{
			START_PREVIEW_SERVER,
			STOP_PREVIEW_SERVER,
			ADD_LOG_INFO,
			SHUTDOWN
		};
		class TaskData
		{
			public:
				explicit TaskData(const string& p_logInfo) : m_logInfo(p_logInfo) {}
				
				string m_logInfo;
				virtual ~TaskData() { }
		};
		
		mutable CriticalSection cs;
		mutable CriticalSection csRoadMap;
		mutable CriticalSection csDownloadItems;
		mutable CriticalSection csInfo;
		
		Semaphore taskSem;
		
		list<pair<Tasks, TaskData*> > m_tasks;
		int run();
		
		void fail(const string& aError);
		bool checkEvents();
		
		void addTask(Tasks task, TaskData* data)
		{
		   {
		    Lock l(cs);
		    m_tasks.push_back(make_pair(task, data));
		   }
		   taskSem.signal();
		}
		bool _serverStarted;
		
		void _StartServer();
		void _StopServer();
		void _ServerStartedReportNoWait(HWND waitWindow)
		{
			::PostMessage(waitWindow, WM_PREVIEW_SERVER_READY, 0 , 0);
		}
		void _AddLogInfo(const std::string& loginfo);
		void _SendLogInfo()
		{
			if (::IsWindow(_logWnd)) ::PostMessage(_logWnd, WM_PREVIEW_SERVER_LOG, 0 , 0);
		}
		
		class PreviewServer : public BASE_THREAD
		{
			public:
				PreviewServer(uint16_t port, const string& ip = "0.0.0.0");
				uint16_t getPort() const
				{
					return port;
				}
				~PreviewServer()
				{
					die = true;
					join();
				}
			private:
				int run() noexcept;
				
				Socket m_sock;
				uint16_t port;
				string ip;
				volatile bool die; // [!] IRainman fix: this variable is volatile.
		};
		
		friend class PreviewServer;
		PreviewServer* _serverPreview;
		bool _isServerDie;
		void accept(const Socket& sock) noexcept;
		bool _isAcceptConnection;
		std::string _currentFilePreview;
		int64_t _previewFileSize;
		string m_tempFilename;
		HWND _callWnd;
		HWND _logWnd;
		bool _canUseFile;
		bool _viewStarted;
		
		unique_ptr<FileRoadMap> _fileRoadMap;
		
		SocketProcessorS _socketProcessors;
		
		MapVItems _ask2Download;
		
		LogInfoDataStack _logInfoData;
		
	private:
		// QueueManagerListener
		void on(QueueManagerListener::FileMoved, const string& n) noexcept; // [IntelC++ 2012 beta2] warning #1125: overloaded function "SettingsManagerListener::on" is hidden by "VideoPreview::on" -- virtual function override intended?
		void on(QueueManagerListener::TryFileMoving, const string& n) noexcept; // [IntelC++ 2012 beta2] warning #1125: overloaded function "SettingsManagerListener::on" is hidden by "VideoPreview::on" -- virtual function override intended?
		
};
#endif // SSA_VIDEO_PREVIEW_FEATURE

#endif // _VIDEO_PREVIEW_H_