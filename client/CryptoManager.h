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

#if !defined(CRYPTO_MANAGER_H)
#define CRYPTO_MANAGER_H

#include "SettingsManager.h"
#include "SSLSocket.h"

STANDARD_EXCEPTION(CryptoException);

class File;
class FileException;

class CryptoManager : public Singleton<CryptoManager>
{
	public:
		string makeKey(const string& aLock);
		const string& getLock() const
		{
			return lock;
		}
		const string& getPk()  const
		{
			return pk;
		}
		static bool isExtended(const string& aLock)
		{
			return strncmp(aLock.c_str(), "EXTENDEDPROTOCOL", 16) == 0;
		}
		
		void decodeBZ2(const uint8_t* is, size_t sz, string& os);
		
		SSLSocket* getClientSocket(bool allowUntrusted);
		SSLSocket* getServerSocket(bool allowUntrusted);
		const vector<uint8_t>& getKeyprint() const noexcept;
		
		void generateCertificate() ;
		
		void loadCertificates() noexcept;
		bool checkCertificate() noexcept;
		
		bool TLSOk() const noexcept;
		
#ifdef HEADER_OPENSSLV_H
		static void __cdecl locking_function(int mode, int n, const char *file, int line);
#endif
		
		
	private:
	
		friend class Singleton<CryptoManager>;
		
		CryptoManager();
		~CryptoManager();
		
		SSL_CTX* clientContext;
		SSL_CTX* clientVerContext;
		SSL_CTX* serverContext;
		SSL_CTX* serverVerContext;
		
#ifdef HEADER_OPENSSLV_H
		ssl::DH dh;
#endif
		bool certsLoaded;
		
		vector<uint8_t> keyprint;
		const string lock;
		const string pk;
		
#ifdef HEADER_OPENSSLV_H
		static CriticalSection* cs;
#endif
		
		string keySubst(const uint8_t* aKey, size_t len, size_t n);
		static bool isExtra(uint8_t b)
		{
			return (b == 0 || b == 5 || b == 124 || b == 96 || b == 126 || b == 36);
		}
		
		void loadKeyprint() noexcept;
		static FILE* openSertFile() noexcept;
};

#endif // !defined(CRYPTO_MANAGER_H)

/**
 * @file
 * $Id: CryptoManager.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
