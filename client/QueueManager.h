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

#ifndef DCPLUSPLUS_DCPP_QUEUE_MANAGER_H
#define DCPLUSPLUS_DCPP_QUEUE_MANAGER_H

#include "ClientManager.h"
#include "Exception.h"
#include "QueueManagerListener.h"
#include "SearchManagerListener.h"
#include "LogManager.h"

STANDARD_EXCEPTION(QueueException);

class UserConnection;

class ConnectionQueueItem;
class QueueLoader;

class QueueManager : public Singleton<QueueManager>, public Speaker<QueueManagerListener>, private TimerManagerListener,
	private SearchManagerListener, private ClientManagerListener
{
	public:
		/** Add a file to the queue. */
		void add(const string& aTarget, int64_t aSize, const TTHValue& root, const UserPtr& aUser,
		         Flags::MaskType aFlags = 0, bool addBad = true, bool p_first_file = true) throw(QueueException, FileException);
		/** Add a user's filelist to the queue. */
		void addList(const UserPtr& aUser, Flags::MaskType aFlags, const string& aInitialDir = Util::emptyString) throw(QueueException, FileException);
		
		//[+] SSA check user IP
		void addCheckUserIP(const UserPtr& aUser) noexcept
		{
			add(Util::emptyString, -1, TTHValue(), aUser, QueueItem::FLAG_USER_GET_IP);
		}
		// [+] IRainman dclst support.
		void addDclst(const string& p_dclstFile) throw(QueueException, FileException)
		{
			dclstLoader.addTask(p_dclstFile);
		}
	private:
		class DclstLoader : public BackgroundTaskExecuter<string>
#ifdef _DEBUG
			, virtual NonDerivable<DclstLoader>
#endif
		{
			public:
				explicit DclstLoader() { }
				~DclstLoader() { }
			private:
				void execute(const string && p_dclstFile);
		} dclstLoader;
		// [~] IRainman dclst support.
	public:
		// [+] FlylinkDC
		bool isQueueItem(const TTHValue& tth) const
		{
			// [-] Lock l(cs); [-] IRainman fix.
			return fileQueue.isQueueItem(tth);
		}
		class LockFileQueueShared
		{
			public:
				// [!] IRainman fix.
				LockFileQueueShared()
				{
					QueueManager::getInstance()->fileQueue.csFQ.lockShared();
				}
				~LockFileQueueShared()
				{
					QueueManager::getInstance()->fileQueue.csFQ.unlockShared();
				}
				// [~] IRainman fix.
				const QueueItem::QIStringMap& getQueue() noexcept
				{
					return QueueManager::getInstance()->fileQueue.getQueueL(); // “ÛÚ L?
				}
		};
		
		class DirectoryListInfo // [+] IRainman fix: moved from MainFrame to core.
		{
			public:
				DirectoryListInfo(const HintedUser& aUser, const string& aFile, const string& aDir, int64_t aSpeed, bool aIsDCLST = false) : m_hintedUser(aUser), file(aFile), dir(aDir), speed(aSpeed), isDCLST(aIsDCLST) { }
				const HintedUser m_hintedUser;
				const string file;
				const string dir;
				const int64_t speed;
				const bool isDCLST;
		};
		typedef DirectoryListInfo* DirectoryListInfoPtr;
		
		void matchAllFileLists() // [+] IRainman fix.
		{
			m_listMatcher.addTask(File::findFiles(Util::getListPath(), "*.xml*"));
		}
		
	private:
	
		class ListMatcher : public BackgroundTaskExecuter<StringList, Thread::LOW, 15000> // [<-] IRainman fix: moved from MainFrame to core.
		{
				void execute(const StringList && list);
		} m_listMatcher;
		
		class FileListQueue: public BackgroundTaskExecuter<DirectoryListInfoPtr, Thread::LOW, 15000> // [<-] IRainman fix: moved from MainFrame to core.
		{
				void execute(const DirectoryListInfoPtr && list);
		} m_listQueue;
		
		// [+] IRainman: auto pausing running downloads before moving.
		struct WaiterFile
		{
			public:
				WaiterFile(): priority(QueueItem::NORMAL) {}
				WaiterFile(const string& p_source, const string& p_target, QueueItem::Priority priority) :
					source(p_source), target(p_target), priority(priority)
				{}
				const string& getSource() const
				{
					return source;
				}
				const string& getTarget() const
				{
					return target;
				}
				QueueItem::Priority getPriority() const
				{
					return priority;
				}
			private:
				string source;
				string target;
				QueueItem::Priority priority;
		};
		class QueueManagerWaiter : public BackgroundTaskExecuter<WaiterFile>
#ifdef _DEBUG
			, virtual NonDerivable<QueueManagerWaiter>
#endif
		{
			public:
				explicit QueueManagerWaiter() { }
				~QueueManagerWaiter() { }
				
				void move(const string& p_source, const string& p_target, QueueItem::Priority p_Priority)
				{
					addTask(WaiterFile(p_source, p_target, p_Priority));
				}
			private:
				void execute(const WaiterFile && p_waiterFile);
		} waiter;
		// [~] IRainman: auto pausing running downloads before moving.
	public:
		//[+] SSA check if file exist
		void setOnDownloadSetting(int p_option)
		{
			m_curOnDownloadSettings = p_option;
		}
		
		void shutdown() // [+] IRainman opt.
		{
			m_listMatcher.forceStop();
			m_listQueue.forceStop();
			waiter.forceStop();
			dclstLoader.forceStop();
			mover.forceStop();
			rechecker.forceStop();
		}
		//[~] FlylinkDC
		
		/** Readd a source that was removed */
		void readd(const string& target, const UserPtr& aUser) throw(QueueException);
		void readdAll(const QueueItemPtr& q) throw(QueueException); // [+] IRainman opt.
		/** Add a directory to the queue (downloads filelist and matches the directory). */
		void addDirectory(const string& aDir, const UserPtr& aUser, const string& aTarget,
		                  QueueItem::Priority p = QueueItem::DEFAULT) noexcept;
		                  
		int matchListing(const DirectoryListing& dl) noexcept;
	private:
		typedef std::unordered_map<TTHValue, const DirectoryListing::File*> TTHMap; // TODO boost
		static void buildMap(const DirectoryListing::Directory* dir, TTHMap& tthMap) noexcept;
	public:
	
		bool getTTH(const string& p_target, TTHValue& p_tth) const
		{
			return fileQueue.getTTH(p_target, p_tth);
		}
		
		/** Move the target location of a queued item. Running items are silently ignored */
		void move(const string& aSource, const string& aTarget) noexcept;
		
		void remove(const string& aTarget) noexcept;
		void removeSource(const string& aTarget, const UserPtr& aUser, Flags::MaskType reason, bool removeConn = true) noexcept;
		void removeSource(const UserPtr& aUser, Flags::MaskType reason) noexcept;
		
		void recheck(const string& aTarget);
		
		void setPriority(const string& aTarget, QueueItem::Priority p) noexcept;
		void setAutoPriority(const string& aTarget, bool ap) noexcept;
		
		void getTargets(const TTHValue& tth, StringList& sl);
#ifdef _DEBUG
		bool isSourceValid(const QueueItemPtr& p_qi, const QueueItem::Source* p_source_ptr)
		{
			SharedLock l(QueueItem::cs); // [+] IRainman fix.
			for (auto i = p_qi->getSourcesL().cbegin(); i != p_qi->getSourcesL().cend(); ++i)
			{
				if (p_source_ptr == &*i)
					return true;
			}
			return false;
		}
#endif
		static size_t countOnlineUsersL(const QueueItemPtr& p_qi) //[+]FlylinkDC++ Team
		{
			// [!] IRainman fix done https://www.box.net/shared/ec264696b10d3fa873e7
			return p_qi->countOnlineUsersL();
		}
		static size_t getSourcesCountL(const QueueItemPtr& qi) // [!] IRainman fix.
		{
			return qi->getSourcesL().size();
		}
		static void getChunksVisualisation(const QueueItemPtr& qi, vector<pair<Segment, Segment>>& p_runnigChunksAndDownloadBytes, vector<Segment>& p_doneChunks) // [!] IRainman fix.
		{
			/* [-] IRainman fix.
			   [-] Lock l(cs);
			   [-] if (qi)
			   [-] */
			qi->getChunksVisualisation(p_runnigChunksAndDownloadBytes, p_doneChunks);
		}
		
		bool getQueueInfo(const UserPtr& aUser, string& aTarget, int64_t& aSize, int& aFlags) noexcept;
		Download* getDownload(UserConnection& aSource, string& aMessage) noexcept;
		void putDownload(Download* aDownload, bool finished, bool reportFinish = true) noexcept;
		void setFile(Download* download);
		
		/** @return The highest priority download the user has, PAUSED may also mean no downloads */
		QueueItem::Priority hasDownload(const UserPtr& aUser) noexcept;
		
		void loadQueue() noexcept;
		void saveQueue(bool force = false) noexcept;
		
		void noDeleteFileList(const string& path);
		
		bool handlePartialSearch(const TTHValue& tth, PartsInfo& _outPartsInfo);
		bool handlePartialResult(const UserPtr& aUser, const TTHValue& tth, const QueueItem::PartialSource& partialSource, PartsInfo& outPartialInfo);
		
#ifdef PPA_INCLUDE_DROP_SLOW
		bool dropSource(Download* d);
#endif
	private:
		void calcPriorityAndGetRunningFiles(QueueItem::PriorityArray& p_proir_array, QueueItemList& p_running_file)
		{
			fileQueue.calcPriorityAndGetRunningFiles(p_proir_array, p_running_file);
		}
		size_t getRunningFileCount() const //[+]PPA opt.
		{
			return fileQueue.getRunningFileCount();
		}
	public:
		bool getTargetByRoot(const TTHValue& tth, string& target, string& tempTarget)
		{
			// [-] Lock l(cs); [-] IRainman fix.
#ifdef IRAINMAN_FASTS_QUEUE_MANAGER
			QueueItemPtr qi = fileQueue.find(tth);
			if (!qi)
				return false;
#else
			QueueItemList ql;
			fileQueue.find(ql, tth);
				
			if (ql.empty())
				return false;
				
			dcassert(ql.size() == 1); // [+] IRainman fix.
				
			const QueueItemPtr& qi = ql.front();
#endif // IRAINMAN_FASTS_QUEUE_MANAGER
				
			target = qi->getTarget();
			tempTarget = qi->getTempTarget();
			return true;
		}
		bool isChunkDownloaded(const TTHValue& tth, int64_t startPos, int64_t& bytes, string& target)
		{
			// [-] Lock l(cs); [-] IRainman fix.
#ifdef IRAINMAN_FASTS_QUEUE_MANAGER
			QueueItemPtr qi = fileQueue.find(tth);
			if (!qi)
				return false;
#else
			QueueItemList ql;
			fileQueue.find(ql, tth);
				
			if (ql.empty())
				return false;
				
			dcassert(ql.size() == 1); // [+] IRainman fix.
				
			const QueueItemPtr& qi = ql.front();
#endif // IRAINMAN_FASTS_QUEUE_MANAGER
				
			SharedLock l(QueueItem::cs);
			target = qi->isFinishedL() ? qi->getTarget() : qi->getTempTarget();
			
			return qi->isChunkDownloadedL(startPos, bytes);
		}
		/** Sanity check for the target filename */
		static string checkTarget(const string& aTarget, const int64_t aSize = -1) throw(QueueException, FileException);
		/** Add a source to an existing queue item */
		bool addSourceL(const QueueItemPtr& qi, const UserPtr& aUser, Flags::MaskType addBad) throw(QueueException, FileException); // [!] IRainman fix.
	private:
		GETSET(uint64_t, lastSave, LastSave);
		// [!] IRainman opt.
		// [-] GETSET(string, queueFile, QueueFile);
		static string getQueueFile()
		{
			return Util::getConfigPath() + "Queue.xml";
		}
		// [~] IRainman opt.
		bool exists_queueFile;
		
		enum { MOVER_LIMIT = 10 * 1024 * 1024 };
		class FileMover : public BackgroundTaskExecuter<pair<string, string>> // [!] IRainman core.
#ifdef _DEBUG
			                                                                   , virtual NonDerivable<FileMover>  // [+] IRainman fix.
#endif
		{
			public:
				explicit FileMover() { }
				~FileMover() { }
				
				void moveFile(const string& source, const string& target)
				{
					addTask(make_pair(source, target));
				}
			private:
				void execute(const pair<string, string> && p_next)
				{
					internal_moveFile(p_next.first, p_next.second);
				}
		} mover;
		
		typedef vector<pair<QueueItem::SourceConstIter, const QueueItemPtr> > PFSSourceList;
		
		class Rechecker : public BackgroundTaskExecuter<string> // [!] IRainman core.
#ifdef _DEBUG
			, virtual NonDerivable<Rechecker>  // [+] IRainman fix.
#endif
		{
				struct DummyOutputStream : OutputStream
#ifdef _DEBUG
						, virtual NonDerivable<DummyOutputStream> // [+] IRainman fix.
#endif
				{
					size_t write(const void*, size_t n)
					{
						return n;
					}
					size_t flush()
					{
						return 0;
					}
				};
			public:
				explicit Rechecker(QueueManager* qm_) : qm(qm_) { }
				~Rechecker() { }
				
				void add(const string& p_file)
				{
					addTask(p_file);
				}
			private:
				void execute(const string && p_file);
				QueueManager* qm;
		} rechecker;
		
		/** All queue items by target */
		class FileQueue
			: private SettingsManagerListener // [+] IRainman opt.
		{
			public:
				FileQueue();
				~FileQueue();
				void add(const QueueItemPtr& qi); // [!] IRainman fix.
				QueueItemPtr add(const string& aTarget, int64_t aSize, Flags::MaskType aFlags, QueueItem::Priority p,
				                 const string& aTempTarget, time_t aAdded, const TTHValue& root) throw(QueueException, FileException); // [!] IRainman fix.
				bool getTTH(const string& p_name, TTHValue& p_tth) const;
				QueueItemPtr find(const string& p_target) const;
				void find(QueueItemList& sl, int64_t aSize, const string& ext) const;
				void find(StringList& sl, int64_t aSize, const string& ext) const;
				void find(QueueItemList& ql, const TTHValue& tth) const;
				QueueItemPtr find(const TTHValue& tth) const; // [+] IRainman opt.
				static uint8_t getMaxSegments(const uint64_t filesize);
				bool isQueueItem(const TTHValue& tth) const; // [+] IRainman opt.
				// find some PFS sources to exchange parts info
				void findPFSSourcesL(PFSSourceList&);
				
#ifdef STRONG_USE_DHT
				// return a PFS tth to DHT publish
				TTHValue* findPFSPubTTH();
#endif
				QueueItemPtr findAutoSearch(deque<string>& recent) const; // [!] IRainman fix.
				size_t getSize() const
				{
					return m_queue.size();
				}
				QueueItem::QIStringMap& getQueueL()
				{
					return m_queue;
				}
				void calcPriorityAndGetRunningFiles(QueueItem::PriorityArray& p_changedPriority, QueueItemList& p_runningFiles);
				size_t getRunningFileCount() const;
				void move(const QueueItemPtr& qi, const string& aTarget); // [!] IRainman fix.
				void remove(const QueueItemPtr& qi); // [!] IRainman fix.
				
				bool empty() const // [+] IRainman opt.
				{
					return m_queue.empty();
				}
				
#ifdef IRAINMAN_USE_SEPARATE_CS_IN_QUEUE_MANAGER
				mutable SharedCriticalSection csFQ; // [+] IRainman fix.
#else
				mutable SharedCriticalSection& csFQ; // [+] IRainman fix.
#endif
				
			private:
				QueueItem::QIStringMap m_queue;
				
				// [+] IRainman opt.
				void on(SettingsManagerListener::QueueChanges) noexcept;
				
				void on(SettingsManagerListener::Load, SimpleXML& /*xml*/) noexcept
				{
					on(SettingsManagerListener::QueueChanges());
				}
				
				void getUserSettingsPriority(const string& target, QueueItem::Priority& p_prio) const;
				
				StringList m_highPrioFiles;
				StringList m_lowPrioFiles;
				mutable FastCriticalSection m_csPriorities;
				// [+] IRainman opt.
		};
		
		/** QueueItems by target */
		FileQueue fileQueue;
		
		/** All queue items indexed by user (this is a cache for the FileQueue really...) */
		class UserQueue
		{
			public:
				void add(const QueueItemPtr& qi); // [!] IRainman fix.
				void addL(const QueueItemPtr& qi, const UserPtr& aUser); // [!] IRainman fix.
				QueueItemPtr getNextL(const UserPtr& aUser, QueueItem::Priority minPrio = QueueItem::LOWEST, int64_t wantedSize = 0, int64_t lastSpeed = 0, bool allowRemove = false); // [!] IRainman fix.
				QueueItemPtr getRunningL(const UserPtr& aUser); // [!] IRainman fix.
				void addDownloadL(const QueueItemPtr& qi, Download* d); // [!] IRainman fix: this function needs external lock.
				void removeDownloadL(const QueueItemPtr& qi, const UserPtr& d); // [!] IRainman fix: this function needs external lock.
				void removeL(const QueueItemPtr& qi, bool removeRunning = true); // [!] IRainman fix.
				void removeL(const QueueItemPtr& qi, const UserPtr& aUser, bool removeRunning = true); // [!] IRainman fix.
				void setPriority(const QueueItemPtr& qi, QueueItem::Priority p); // [!] IRainman fix.
				// [+] IRainman fix.
				typedef boost::unordered_map<UserPtr, QueueItemList> UserQueueMap;
				typedef boost::unordered_map<UserPtr, QueueItemPtr> RunningMap;
				/* [-]
				   [-]const RunningMap& getRunning() const
				   [-]{
				   [-]  return running;
				   [-]}
				   [-]*/
				// [~] IRainman fix.
#ifdef IRAINMAN_NON_COPYABLE_USER_QUEUE_ON_USER_CONNECTED_OR_DISCONECTED
				const UserQueueMap& getListL(size_t i) const
				{
					return userQueue[i];
				}
#else
				bool userIsDownloadedFiles(const UserPtr& aUser, QueueItemList& p_status_update_array) noexcept
				{
					bool hasDown = false;
					SharedLock l(QueueItem::cs); // [!] IRainman fix. DeadLock[3]
					for (size_t i = 0; i < QueueItem::LAST; ++i)
					{
						const auto j = userQueue[i].find(aUser);
						if (j != userQueue[i].end())
						{
							p_status_update_array.insert(p_status_update_array.end(), j->second.cbegin(), j->second.cend());
							if (i != QueueItem::PAUSED)
								hasDown = true;
						}
					}
					return hasDown;
				}
#endif // IRAINMAN_NON_COPYABLE_USER_QUEUE_ON_USER_CONNECTED_OR_DISCONECTED
				
				void getLastError(string& out)
				{
					out.clear();
					swap(out, lastError);
				}
				
			private:
				/** QueueItems by priority and user (this is where the download order is determined) */
				UserQueueMap userQueue[QueueItem::LAST];
				/** Currently running downloads, a QueueItem is always either here or in the userQueue */
				RunningMap running;
				/** Last error message to sent to TransferView */
				string lastError;
		};
		
		friend class QueueLoader;
		friend class Singleton<QueueManager>;
		
		QueueManager();
		~QueueManager();
		
		mutable FastCriticalSection csDirectories; // [!] IRainman fix.
		
		/** QueueItems by user */
		UserQueue userQueue;
		/** Directories queued for downloading */
		boost::unordered_multimap<UserPtr, DirectoryItemPtr> directories;
		/** Recent searches list, to avoid searching for the same thing too often */
		deque<string> recent;
		/** The queue needs to be saved */
		bool dirty;
		/** Next search */
		uint64_t nextSearch;
		/** File lists not to delete */
		StringList protectedFileLists;
		
		void processList(const string& name, const HintedUser& hintedUser, int flags);
		
		void load(const SimpleXML& aXml);
		void moveFile(const string& source, const string& target);
		static void internal_moveFile(const string& source, const string& target);
		void moveStuckFile(const QueueItemPtr& qi); // [!] IRainman fix.
		void rechecked(const QueueItemPtr& qi); // [!] IRainman fix.
		
		void setDirty();
		
		string getListPath(const UserPtr& user) const;
		
		// TimerManagerListener
		void on(TimerManagerListener::Second, uint64_t aTick) noexcept;
		void on(TimerManagerListener::Minute, uint64_t aTick) noexcept;
		
		// SearchManagerListener
		void on(SearchManagerListener::SR, const SearchResultPtr&) noexcept;
		
		// ClientManagerListener
		void on(ClientManagerListener::UserConnected, const UserPtr& aUser) noexcept;
		void on(ClientManagerListener::UserDisconnected, const UserPtr& aUser) noexcept;
		
		//[+] SSA check if file exist
		int m_curOnDownloadSettings;
};

#endif // !defined(QUEUE_MANAGER_H)

/**
 * @file
 * $Id: QueueManager.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
