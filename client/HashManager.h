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

#ifndef DCPLUSPLUS_DCPP_HASH_MANAGER_H
#define DCPLUSPLUS_DCPP_HASH_MANAGER_H

#include "Semaphore.h"
#include "TimerManager.h"
#include "Streams.h"
#include "CFlyMediaInfo.h"

#ifdef RIP_USE_STREAM_SUPPORT_DETECTION
#include "FsUtils.h"
#endif

#define IRAINMAN_NTFS_STREAM_TTH

#define MAX_HASH_PROGRESS_VALUE 3000 //[+] brain-ripper

STANDARD_EXCEPTION(HashException);
class File;

class HashManagerListener
{
	public:
		virtual ~HashManagerListener() { }
		template<int I> struct X
		{
			enum { TYPE = I };
		};
		
		typedef X<0> TTHDone;
		
		virtual void on(TTHDone, const string& /* fileName */, const TTHValue& /* root */,
		                int64_t aTimeStamp, const CFlyMediaInfo& p_out_media, int64_t p_size) noexcept = 0;
};

class FileException;

class HashManager : public Singleton<HashManager>, public Speaker<HashManagerListener>,
	private TimerManagerListener
{
	public:
	
		HashManager()
		{
			TimerManager::getInstance()->addListener(this);
		}
		virtual ~HashManager() noexcept
		{
			TimerManager::getInstance()->removeListener(this);
			hasher.join();
		}
		
		void hashFile(const string& fileName, int64_t aSize)
		{
			// Lock l(cs); [-] IRainman fix: this lock is epic fail...
			hasher.hashFile(fileName, aSize);
		}
		
		/**
		 * Check if the TTH tree associated with the filename is current.
		 */
		bool checkTTH(const string& fname, const string& fpath, int64_t p_path_id, int64_t aSize, int64_t aTimeStamp, TTHValue& p_out_tth);
		void stopHashing(const string& baseDir)
		{
			hasher.stopHashing(baseDir);
		}
		void setPriority(Thread::Priority p)
		{
			hasher.setThreadPriority(p);
		}
		
		/** @return TTH root */
		const TTHValue getTTH(const string& fname, const string& fpath, int64_t aSize) throw(HashException);
		
		void addTree(const string& aFileName, uint64_t aTimeStamp, const TigerTree& tt, int64_t p_Size)
		{
			hashDone(aFileName, aTimeStamp, tt, -1, false, p_Size);
		}
		void addTree(const TigerTree& p_tree);
		
		void getStats(string& curFile, int64_t& bytesLeft, size_t& filesLeft)
		{
			hasher.getStats(curFile, bytesLeft, filesLeft);
		}
		
		/**
		 * Rebuild hash data file
		 */
		void rebuild()
		{
			hasher.scheduleRebuild();
		}
		
		void startup()
		{
			hasher.start(0);
		}
		
		void shutdown()
		{
			// [-] brain-ripper
			// Removed critical section - it caused deadlock on client exit:
			// "join" hangs on WaitForSingleObject(threadHandle, INFINITE),
			// and threadHandle thread hangs on this very critical section
			// in HashManager::hashDone
			//Lock l(cs); //[!]IRainman
			
			hasher.shutdown();
			hasher.join();
		}
		bool getMediaInfo(const string& p_name, CFlyMediaInfo& p_media, int64_t p_size, const TTHValue& p_tth, bool p_force = false);
		
		struct HashPauser
		{
				HashPauser();
				~HashPauser();
				
			private:
				bool resume;
		};
		
		/// @return whether hashing was already paused
		bool pauseHashing();
		void resumeHashing();
		bool isHashingPaused() const;
		
		
//[+] Greylink
#ifdef IRAINMAN_NTFS_STREAM_TTH
		class StreamStore   // greylink dc++: work with ntfs stream
		{
			public:
				bool loadTree(const string& p_filePath, TigerTree& p_Tree, int64_t p_aFileSize = -1);
				bool saveTree(const string& p_filePath, const TigerTree& p_Tree);// [+] IRainman const string& p_filePath
				void deleteStream(const string& p_filePath);//[+] IRainman
				
#ifdef RIP_USE_STREAM_SUPPORT_DETECTION
				void SetFsDetectorNotifyWnd(HWND hWnd);
				void OnDeviceChange(LPARAM lParam, WPARAM wParam)
				{
					m_FsDetect.OnDeviceChange(lParam, wParam);
				}
#endif
			private:
				struct TTHStreamHeader
				{
					uint32_t magic;
					uint32_t checksum;  // xor of other TTHStreamHeader DWORDs
					uint64_t fileSize;
					uint64_t timeStamp;
					uint64_t blockSize;
					TTHValue root;
				};
				static const uint32_t g_MAGIC = '++lg'; // warning #1899: multicharacter character literal (potential portability problem)
				static const string g_streamName;
#ifndef RIP_USE_STREAM_SUPPORT_DETECTION
				static std::unordered_set<char> g_error_tth_stream; //[+]PPA
				static void addBan(const string& p_filePath)
				{
					if (p_filePath.size())
					{
						//FastLock l(m_cs); // [!] IRainman fix.
						g_error_tth_stream.insert(p_filePath[0]);
					}
				}
				static bool isBan(const string& p_filePath)
				{
					//FastLock l(m_cs); // [!] IRainman fix.
					return p_filePath.size() && g_error_tth_stream.find(p_filePath[0]) != g_error_tth_stream.end(); // [!] IRainman opt.
				}
				//FastCriticalSection m_cs; // [+] IRainman fix.
#endif
				
				void setCheckSum(TTHStreamHeader& p_header);
				bool validateCheckSum(const TTHStreamHeader& p_header);
#ifdef RIP_USE_STREAM_SUPPORT_DETECTION
				// [+] brain-ripper
				// Detector if volume support streams
				FsUtils::CFsTypeDetector m_FsDetect;
#endif
		};
//[~] Greylink
		StreamStore m_streamstore;
#endif // IRAINMAN_NTFS_STREAM_TTH
		
#ifdef RIP_USE_STREAM_SUPPORT_DETECTION
		void SetFsDetectorNotifyWnd(HWND hWnd);
		void OnDeviceChange(LPARAM lParam, WPARAM wParam)
		{
			m_streamstore.OnDeviceChange(lParam, wParam);
		}
#endif
		
		// [+] brain-ripper
		// Temporarily change hash speed functional
		inline bool IsHashing()
		{
			return hasher.IsHashing();
		}
		inline size_t GetProgressValue()
		{
			return hasher.GetProgressValue();
		}
		inline uint64_t GetStartTime()
		{
			return hasher.GetStartTime();
		}
		inline void GetProcessedFilesAndBytesCount(int64_t &bytes, size_t &files)
		{
			hasher.GetProcessedFilesAndBytesCount(bytes, files);
		}
		static inline DWORD GetMaxProgressValue()
		{
			return Hasher::GetMaxProgressValue();
		}
		inline void EnableForceMinHashSpeed(int iMinHashSpeed)
		{
			hasher.EnableForceMinHashSpeed(iMinHashSpeed);
		}
		inline void DisableForceMinHashSpeed()
		{
			hasher.DisableForceMinHashSpeed();
		}
		inline int GetMaxHashSpeed()
		{
			return hasher.GetMaxHashSpeed();
		}
		// [~] brain-ripper
		
		
	private:
		class Hasher : public Thread
		{
			public:
				Hasher() : stop(false), running(false), paused(0), rebuild(false), currentSize(0),
					m_CurrentBytesLeft(0), //[+]IRainman
					m_ForceMaxHashSpeed(0), dwMaxFiles(0), iMaxBytes(0), uiStartTime(0) { }
					
				void hashFile(const string& fileName, int64_t size);
				
				/// @return whether hashing was already paused
				bool pause();
				void resume();
				bool isPaused() const;
				
				void stopHashing(const string& baseDir);
				int run();
				bool fastHash(const string& fname, uint8_t* buf, TigerTree& tth, int64_t size);
				// [+] brain-ripper
				void getStats(string& curFile, int64_t& bytesLeft, size_t& filesLeft)
				{
					FastLock l(cs);
					curFile = m_fname;
					getBytesAndFileLeft(bytesLeft, filesLeft);
				}
				
				void signal()
				{
					if (paused)
					{
						m_s.signal(); // TODO тут точно нужен такой двойной сигнал?
					}
					m_s.signal();
				}
				void shutdown()
				{
					stop = true;
					signal();
				}
				void scheduleRebuild()
				{
					rebuild = true;
					signal();
				}
				
				// [+] brain-ripper: Temporarily change hash speed functional
				int GetMaxHashSpeed()
				{
					return m_ForceMaxHashSpeed != 0 ? m_ForceMaxHashSpeed : SETTING(MAX_HASH_SPEED);
				}
			private:
				inline void getBytesAndFileLeft(int64_t& bytesLeft, size_t& filesLeft)
				{
					filesLeft = w.size();
					if (running)
						filesLeft++;
						
					bytesLeft = currentSize + m_CurrentBytesLeft; // [!]IRainman
				}
			public:
				inline void EnableForceMinHashSpeed(int iMinHashSpeed)
				{
					m_ForceMaxHashSpeed = iMinHashSpeed;
				}
				inline void DisableForceMinHashSpeed()
				{
					m_ForceMaxHashSpeed = 0;
				}
				// [~] brain-ripper: Temporarily change hash speed functional
				
				inline bool IsHashing()
				{
					return running;
				}
				
				inline size_t GetProgressValue()
				{
					if (!running || dwMaxFiles == 0 || iMaxBytes == 0)
						return 0;
						
					int64_t bytesLeft;
					size_t filesLeft;
					{
						FastLock l(cs);
						getBytesAndFileLeft(bytesLeft, filesLeft);
					}
					
					return static_cast<size_t>(MAX_HASH_PROGRESS_VALUE * ((static_cast<double>(dwMaxFiles - filesLeft + 1) / static_cast<double>(dwMaxFiles)) * (static_cast<double>(iMaxBytes - bytesLeft) / static_cast<double>(iMaxBytes))));
				}
				
				inline uint64_t GetStartTime()
				{
					return IsHashing() ? uiStartTime : 0;
				}
				
				inline void GetProcessedFilesAndBytesCount(int64_t &bytes, size_t &files)
				{
					if (IsHashing())
					{
						int64_t bytesLeft;
						size_t filesLeft;
						{
							FastLock l(cs);
							getBytesAndFileLeft(bytesLeft, filesLeft);
						}
						bytes = iMaxBytes >= bytesLeft ? iMaxBytes - bytesLeft : iMaxBytes;
						files = dwMaxFiles >= filesLeft ? dwMaxFiles - filesLeft : dwMaxFiles;
					}
					else
					{
						bytes = 0;
						files = 0;
					}
				}
				
				static inline DWORD GetMaxProgressValue()
				{
					return MAX_HASH_PROGRESS_VALUE;
				}
				// [+] brain-ripper
				// end of Temporarily change hash speed functional
				
			private:
				// Case-sensitive (faster), it is rather unlikely that case changes, and if it does it's harmless.
				// map because it's sorted (to avoid random hash order that would create quite strange shares while hashing)
				typedef std::map<string, int64_t> WorkMap;
				
				WorkMap w;
				mutable FastCriticalSection cs; // [!] IRainman opt: use only spinlock here!
				Semaphore m_s;
				
				volatile bool stop; // [!] IRainman fix: this variable is volatile.
				volatile bool running; // [!] IRainman fix: this variable is volatile.
				int64_t paused; //[!] PPA -> int
				volatile bool rebuild; // [!] IRainman fix: this variable is volatile.
				//string currentFile;// [-]IRainman
				int64_t currentSize;
				
				void instantPause();
				
				// [+] brain-ripper
				int m_ForceMaxHashSpeed;
				size_t dwMaxFiles;
				int64_t iMaxBytes;
				uint64_t uiStartTime;
				// -----------------
				
				// [+]IRainman for better performance
				int64_t m_CurrentBytesLeft;
				string m_fname;
				// [~]IRainman
		};
		
		friend class Hasher;
		
		bool addFile(const string& p_file_name, int64_t p_time_stamp, const TigerTree& p_tth, int64_t p_size, CFlyMediaInfo& p_out_media);
#ifdef IRAINMAN_NTFS_STREAM_TTH
		bool addFileFromStream(const string& p_name, const TigerTree& p_TT, int64_t p_size);
#endif
		
		Hasher hasher;
		
		// mutable CriticalSection cs; [-] IRainman fix: no data to lock.
		
		/** Single node tree where node = root, no storage in HashData.dat */
		static const int64_t SMALL_TREE = -1;
		
		void hashDone(const string& aFileName, uint64_t aTimeStamp, const TigerTree& tth, int64_t speed,
		              bool p_is_ntfs,
		              int64_t p_Size);
		void doRebuild()
		{
			// Lock l(cs); [-] IRainman fix: no data to lock.
			rebuild();
		}
};

#endif // !defined(HASH_MANAGER_H)

/**
 * @file
 * $Id: HashManager.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
