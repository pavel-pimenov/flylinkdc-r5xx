/*
 * Copyright (C) 2001-2019 Jacek Sieka, arnetheduck on gmail point com
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

#ifndef DCPLUSPLUS_DCPP_CRYPTO_MANAGER_H
#define DCPLUSPLUS_DCPP_CRYPTO_MANAGER_H

#include "SSL.h"
#include "Socket.h"
#include "Singleton.h"

class SSLSocket;

STANDARD_EXCEPTION(CryptoException);

//This is for earlier OpenSSL versions that don't have this error code yet..
#ifndef X509_V_ERR_UNSPECIFIED
#define X509_V_ERR_UNSPECIFIED 1
#endif

namespace dcpp {


class CryptoManager : public Singleton<CryptoManager>
{
	public:
		typedef std::pair<bool, string> SSLVerifyData;
		
		enum TLSTmpKeys {
			KEY_FIRST = 0,
			KEY_DH_2048 = KEY_FIRST,
			KEY_DH_4096,
			KEY_RSA_2048,
			KEY_LAST
		};
		
		enum SSLContext {
			SSL_CLIENT,
			SSL_SERVER,
			SSL_CLIENT_ALPN
		};
		
		string makeKey(const string& aLock);
		const string& getLock() {
			return lock;
		}
		const string& getPk() {
			return pk;
		}
		static bool isExtended(const string& aLock) {
			return strncmp(aLock.c_str(), "EXTENDEDPROTOCOL", 16) == 0;
		}
		
		SSLSocket* getClientSocket(bool allowUntrusted, Socket::Protocol proto);
		SSLSocket* getServerSocket(bool allowUntrusted);
		
		SSL_CTX* getSSLContext(SSLContext wanted);
		
		void loadCertificates() noexcept;
		void generateCertificate();
		bool checkCertificate(int minValidityDays) noexcept;
		static const ByteVector& getKeyprint() noexcept;
		
		static bool TLSOk() noexcept;
		
		static void locking_function(int mode, int n, const char* /*file*/, int /*line*/);
		static DH* tmp_dh_cb(SSL* /*ssl*/, int /*is_export*/, int keylength);
		static RSA* tmp_rsa_cb(SSL* /*ssl*/, int /*is_export*/, int keylength);
		static int verify_callback(int preverify_ok, X509_STORE_CTX *ctx);
		
		static void setCertPaths();
		
		static int idxVerifyData;
		
		// Options that can also be shared with external contexts
		static void setContextOptions(SSL_CTX* aSSL, bool aServer);
		static string keyprintToString(const ByteVector& aKP) noexcept;
		
		static ByteVector calculateSha1(const string& aData) noexcept;
	private:
	
		friend class Singleton<CryptoManager>;
		
		CryptoManager();
		virtual ~CryptoManager();
		
		ssl::SSL_CTX clientContext;
		ssl::SSL_CTX clientVerContext;
		ssl::SSL_CTX serverContext;
		ssl::SSL_CTX serverVerContext;
		
		ssl::SSL_CTX clientALPNContext;
		ssl::SSL_CTX serverALPNContext;
		
		
		void sslRandCheck();
		
		static int getKeyLength(TLSTmpKeys key);
		static DH* getTmpDH(int keyLen);
		static RSA* getTmpRSA(int keyLen);
		
		static bool certsLoaded;
		
		static void* g_tmpKeysMap[KEY_LAST];
		static CriticalSection* cs;
		static char idxVerifyDataName[];
		static SSLVerifyData trustedKeyprint;
		
		static ByteVector keyprint;
		const string lock;
		const string pk;
		
		string keySubst(const uint8_t* aKey, size_t len, size_t n);
		bool isExtra(uint8_t b) {
			return (b == 0 || b == 5 || b == 124 || b == 96 || b == 126 || b == 36);
		}
		
		static string formatError(X509_STORE_CTX *ctx, const string& message);
		static string getNameEntryByNID(X509_NAME* name, int nid) noexcept;
		
		void loadKeyprint(const string& file) noexcept;
		
};

} // namespace dcpp

using namespace dcpp; // FlylinkDC++

#endif // !defined(CRYPTO_MANAGER_H)
