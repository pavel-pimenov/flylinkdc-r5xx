#include "stdinc.h"
#include "FsUtils.h"
#include <Dbt.h>

#define INITGUID
#include <guiddef.h>
#include <Ioevent.h>

namespace FsUtils
{

bool CFsTypeDetector::IsStreamSupported(FsUtils_LPCtSTR pwcFileName)
{
	size_t szLen = FsUtils_slen(pwcFileName);
	if (szLen >= 3 && FsUtils_sncmp(&pwcFileName[1], FsUtils_L(":\\"), 2) == 0)
	{
		// Filename in form of "C:\some_path\some_filename.ext"
		
		FsUtils_CHAR wcRootPath[] = FsUtils_L("\\\\?\\C\0\\");
		wcRootPath[4] = (FsUtils_CHAR)toupper(*pwcFileName);
		
		//FastLock l(m_cs); // [!] IRainman fix.
		const tCACHE::const_iterator i = m_cache.find(&wcRootPath[4]);
		if (i != m_cache.end())
		{
			return i->second.bSupportStream;
		}
		else
		{
			// No cached value for this volume,
			// make check and save to cache.
			
			wcRootPath[5] = FsUtils_L(':');
			
			DWORD dwVolumeFlags = 0;
			FsUtils_GetVolumeInformation(&wcRootPath[4], NULL, 0, NULL, NULL, &dwVolumeFlags, NULL, 0);
			
			const bool bRet = (dwVolumeFlags & FILE_NAMED_STREAMS) != 0;
			
			DEV_BROADCAST_HANDLE dbh =
			{
				sizeof(DEV_BROADCAST_HANDLE),
				DBT_DEVTYP_HANDLE,
			};
			
			wcRootPath[6] = 0;
			dbh.dbch_handle = FsUtils_CreateFile(wcRootPath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, NULL, NULL);
			
			VOL_STRUCT vs = {bRet, NULL};
			
			if (dbh.dbch_handle != NULL && dbh.dbch_handle != INVALID_HANDLE_VALUE)
			{
				vs.hNotify = RegisterDeviceNotification(m_hWnd, &dbh, DEVICE_NOTIFY_WINDOW_HANDLE);
				
				CloseHandle(dbh.dbch_handle);
			}
			
			wcRootPath[5] = 0;
			m_cache[&wcRootPath[4]] = vs;
			
			return bRet;
		}
	}
	else if (szLen >= 2 && FsUtils_sncmp(pwcFileName, FsUtils_L("\\\\"), 2) == 0)
	{
		// Network path
		// Assume, that path is in form of "\\server_name\share_name\some_path\some_filename.ext"
		// and extract "\\server_name\share_name\" part
		
		const FsUtils_CHAR *p = &pwcFileName[2];
		int iCount = 0;
		
		while (true)
		{
			p = FsUtils_schr(p, FsUtils_L('\\'));
			if (p && iCount < 1)
			{
				iCount++;
				p++;
			}
			else
				break;
		}
		
		if (p)
		{
			FsUtils_str strRemotePath(pwcFileName, p - pwcFileName + 1);
			
			//FastLock l(m_cs); // [!] IRainman fix.
			const tCACHE::const_iterator i = m_cache.find(strRemotePath);
			if (i != m_cache.end())
			{
				return i->second.bSupportStream;
			}
			else
			{
				// Assume, that there is already SMB connection exist and follow function
				// will not take too long
				DWORD dwVolumeFlags = 0;
				FsUtils_GetVolumeInformation(strRemotePath.c_str(), NULL, 0, NULL, NULL, &dwVolumeFlags, NULL, 0);
				
				const bool bRet = (dwVolumeFlags & FILE_NAMED_STREAMS) != 0;
				
				VOL_STRUCT vs = {bRet, NULL};
				m_cache[strRemotePath] = vs;
				
				return bRet;
			}
		}
	}
	
	return true;
}

LRESULT CFsTypeDetector::OnDeviceChange(LPARAM lParam, WPARAM wParam)
{
	switch (wParam)
	{
		case DBT_DEVICEREMOVECOMPLETE:
		case DBT_DEVICEREMOVEPENDING:
		{
			DEV_BROADCAST_HDR *pHdr = (DEV_BROADCAST_HDR*)lParam;
			
			if (pHdr->dbch_devicetype == DBT_DEVTYP_VOLUME)
			{
				DEV_BROADCAST_VOLUME *pVolume = (DEV_BROADCAST_VOLUME *)lParam;
				ULONG ulDriveLettersMask = pVolume->dbcv_unitmask;
				
				ULONG ind = 0;
				FsUtils_CHAR wcDriveLetter[2] = {0};
				
				
				//FastLock l(m_cs); // [!] IRainman fix.
				// Cycle thought removing/removed drive letters and erase them from cache
				while (ulDriveLettersMask)
				{
					BitScanForward(&ind, ulDriveLettersMask);
					
					ulDriveLettersMask &= ~(1 << ind);
					
					wcDriveLetter[0] = FsUtils_CHAR(FsUtils_L('A') + ind);
					
					tCACHE::const_iterator i = m_cache.find(wcDriveLetter);
					if (i != m_cache.end())
					{
						if (i->second.hNotify)
						{
							UnregisterDeviceNotification(i->second.hNotify);
						}
						m_cache.erase(i);
					}
				}
			}
		}
		break;
		
		case DBT_CUSTOMEVENT:
		{
			DEV_BROADCAST_HDR *pHdr = (DEV_BROADCAST_HDR *)lParam;
			if (pHdr->dbch_devicetype == DBT_DEVTYP_HANDLE)
			{
				DEV_BROADCAST_HANDLE *pHandle = (DEV_BROADCAST_HANDLE *)lParam;
				if (IsEqualGUID(pHandle->dbch_eventguid, GUID_IO_VOLUME_DISMOUNT))
				{
					// Probably disk formatting process
					//FastLock l(m_cs); // [!] IRainman fix.
					for (auto i = m_cache.cbegin(); i != m_cache.cend(); ++i)
					{
						if (i->second.hNotify == pHandle->dbch_hdevnotify)
						{
							UnregisterDeviceNotification(i->second.hNotify);
							m_cache.erase(i);
							break;
						}
					}
				}
			}
		}
		break;
	}
	return true;
}

void CFsTypeDetector::SetNotifyWnd(HWND hWnd)
{
	m_hWnd = hWnd;
}

} // namespace FsUtils