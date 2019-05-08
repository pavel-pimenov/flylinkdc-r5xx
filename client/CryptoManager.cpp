/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
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

#include "stdinc.h"
#include "CryptoManager.h"

#include <boost/scoped_array.hpp>

#include "File.h"
#include "LogManager.h"
#include "ClientManager.h"
#include "version.h"

#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include <bzlib.h>

void* CryptoManager::g_tmpKeysMap[KEY_LAST] = { NULL, NULL, NULL };
CriticalSection* CryptoManager::cs = NULL;
int CryptoManager::idxVerifyData = 0;
char CryptoManager::idxVerifyDataName[] = "FlylinkDC.VerifyData";
CryptoManager::SSLVerifyData CryptoManager::trustedKeyprint = { false, "trusted_keyp" };
bool CryptoManager::certsLoaded = false;
ByteVector CryptoManager::keyprint;
static CriticalSection g_cs;

unsigned char alpn_protos[] = {
	3, 'a', 'd', 'c',
	4, 'n', 'm', 'd', 'c',
};

CryptoManager::CryptoManager()
	:
	lock("EXTENDEDPROTOCOLABCABCABCABCABCABC"),
	pk("DCPLUSPLUS"  A_VERSIONSTRING)
{

	const auto l_count_cs = CRYPTO_num_locks();
	cs = new CriticalSection[l_count_cs];
	CRYPTO_set_locking_callback(locking_function);
	
	SSL_library_init();
	SSL_load_error_strings();
	
	clientContext.reset(SSL_CTX_new(SSLv23_client_method()));
	clientALPNContext.reset(SSL_CTX_new(SSLv23_client_method()));
	serverContext.reset(SSL_CTX_new(SSLv23_server_method()));
	
	idxVerifyData = SSL_get_ex_new_index(0, idxVerifyDataName, NULL, NULL, NULL);
	
	if (clientContext && clientALPNContext && serverContext)
	{
		// Check that openssl rng has been seeded with enough data
		sslRandCheck();
		
		// http://www.flylinkdc.ru/2016/06/openssl.html
		// initTmpKeyMaps();
		
		const char ciphersuites[] = "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA384:DHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES256-SHA:ECDHE-RSA-AES256-SHA:DHE-RSA-AES128-SHA:AES128-GCM-SHA256:AES256-GCM-SHA384:AES128-SHA256:AES256-SHA256:AES128-SHA:AES256-SHA";
		SSL_CTX_set_options(clientContext, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION);
		SSL_CTX_set_cipher_list(clientContext, ciphersuites);
		SSL_CTX_set1_curves_list(clientContext, "P-256");
		SSL_CTX_set_options(clientALPNContext, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION);
		SSL_CTX_set_cipher_list(clientALPNContext, ciphersuites);
		SSL_CTX_set1_curves_list(clientALPNContext, "P-256");
		SSL_CTX_set_options(serverContext, SSL_OP_SINGLE_DH_USE | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION);
		SSL_CTX_set_cipher_list(serverContext, ciphersuites);
		SSL_CTX_set1_curves_list(serverContext, "P-256");
		
		EC_KEY* tmp_ecdh;
		if ((tmp_ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1)) != NULL)
		{
			SSL_CTX_set_options(serverContext, SSL_OP_SINGLE_ECDH_USE);
			SSL_CTX_set_tmp_ecdh(serverContext, tmp_ecdh);
			
			EC_KEY_free(tmp_ecdh);
		}
		
		SSL_CTX_set_tmp_dh_callback(serverContext, CryptoManager::tmp_dh_cb);
		SSL_CTX_set_tmp_rsa_callback(serverContext, CryptoManager::tmp_rsa_cb);
		
		SSL_CTX_set_verify(clientContext, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, verify_callback);
		SSL_CTX_set_verify(clientALPNContext, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, verify_callback);
		SSL_CTX_set_verify(serverContext, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, verify_callback);
		
		SSL_CTX_set_alpn_protos(clientALPNContext, alpn_protos, sizeof(alpn_protos));
	}
}
void CryptoManager::initTmpKeyMaps()
{
	static bool g_is_init_tmp_key_map = false;
	if (g_is_init_tmp_key_map == false)
	{
		g_is_init_tmp_key_map = true;
		// Init temp data for DH keys
		for (int i = KEY_FIRST; i != KEY_RSA_2048; ++i)
			if (!g_tmpKeysMap[i])
			{
				g_tmpKeysMap[i] = getTmpDH(getKeyLength(static_cast<TLSTmpKeys>(i)));
			}
			
		// and same for RSA keys
		for (int i = KEY_RSA_2048; i != KEY_LAST; ++i)
		{
			if (!g_tmpKeysMap[i])
			{
				g_tmpKeysMap[i] = getTmpRSA(getKeyLength(static_cast<TLSTmpKeys>(i)));
			}
		}
	}
}

void CryptoManager::freeTmpKeyMaps()
{
	for (int i = KEY_FIRST; i != KEY_RSA_2048; ++i)
	{
		if (g_tmpKeysMap[i])
		{
			DH_free((DH*)g_tmpKeysMap[i]);
		}
	}
	
	for (int i = KEY_RSA_2048; i != KEY_LAST; ++i)
	{
		if (g_tmpKeysMap[i])
		{
			RSA_free((RSA*)g_tmpKeysMap[i]);
		}
	}
}
CryptoManager::~CryptoManager()
{

	/* thread-local cleanup */
	ERR_remove_thread_state(NULL);
	
	clientContext.reset();
	clientALPNContext.reset();
	serverContext.reset();
	
	freeTmpKeyMaps();
	
	/* global application exit cleanup (after all SSL activity is shutdown) */
	SSL_COMP_free_compression_methods();
	
	ERR_free_strings();
	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();
	
	CRYPTO_set_locking_callback(NULL);
	delete[] cs;
}

void CryptoManager::sslRandCheck()
{
	if (!RAND_status())
	{
#ifndef _WIN32
		if (!Util::fileExists("/dev/urandom"))
		{
			// This is questionable, but hopefully we don't end up here
			time_t time = GET_TIME();
			RAND_seed(&time, sizeof(time_t));
			pid_t pid = getpid();
			RAND_seed(&pid, sizeof(pid_t));
			char stackdata[1024];
			RAND_seed(&stackdata, 1024);
		}
#else
		RAND_screen();
#endif
	}
}

string CryptoManager::formatError(X509_STORE_CTX *ctx, const string& message)
{
	CFlyLock(g_cs);
	X509* cert = NULL;
	if ((cert = X509_STORE_CTX_get_current_cert(ctx)) != NULL)
	{
		X509_NAME* subject = X509_get_subject_name(cert);
		string tmp, line;
		
		tmp = getNameEntryByNID(subject, NID_commonName);
		if (!tmp.empty())
		{
			if (tmp.length() < 39)
				tmp.append('x', 39 - tmp.length());
			CID certCID(tmp);
			if (tmp.length() == 39 && !certCID.isZero())
				tmp = Util::toString(ClientManager::getNicks(certCID, Util::emptyString, false));
			line += (!line.empty() ? ", " : "") + tmp;
		}
		else
		{
			dcassert(0);
		}
		
		tmp = getNameEntryByNID(subject, NID_organizationName);
		if (!tmp.empty())
			line += (!line.empty() ? ", " : "") + tmp;
			
		return str(F_("Certificate verification for %1% failed with error: %2%") % line % message);
	}
	
	return Util::emptyString;
}

int CryptoManager::verify_callback(int preverify_ok, X509_STORE_CTX *ctx)
{
	int err = X509_STORE_CTX_get_error(ctx);
	SSL* ssl = (SSL*)X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
	SSLVerifyData* verifyData = (SSLVerifyData*)SSL_get_ex_data(ssl, CryptoManager::idxVerifyData);
	
	// This happens only when KeyPrint has been pinned and we are not skipping errors due to incomplete chains
	// we can fail here f.ex. if the certificate has expired but is still pinned with KeyPrint
	if (!verifyData || SSL_get_shutdown(ssl) != 0)
		return preverify_ok;
		
	bool allowUntrusted = verifyData->first;
	string keyp = verifyData->second;
	string error = Util::emptyString;
	
	if (!keyp.empty())
	{
		X509* cert = X509_STORE_CTX_get_current_cert(ctx);
		if (!cert)
			return 0;
			
		if (keyp.compare(0, 12, "trusted_keyp") == 0)
		{
			// Possible follow up errors, after verification of a partial chain
			if (err == X509_V_ERR_CERT_UNTRUSTED || err == X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE)
			{
				X509_STORE_CTX_set_error(ctx, X509_V_OK);
				return 1;
			}
			
			return preverify_ok;
		}
		else if (keyp.compare(0, 7, "SHA256/") != 0)
			return allowUntrusted ? 1 : 0;
			
		ByteVector kp = X509_digest_internal(cert, EVP_sha256());
		string expected_keyp = "SHA256/" + Encoder::toBase32(&kp[0], kp.size());
		
		// Do a full string comparison to avoid potential false positives caused by invalid inputs
		if (keyp.compare(expected_keyp) == 0)
		{
			// KeyPrint validated, we can get rid of it (to avoid unnecessary passes)
			SSL_set_ex_data(ssl, CryptoManager::idxVerifyData, NULL);
			
			if (err != X509_V_OK)
			{
				X509_STORE* store = X509_STORE_CTX_get0_store(ctx);
				
				// Hide the potential library error about trying to add a dupe
				ERR_set_mark();
				if (X509_STORE_add_cert(store, cert))
				{
					// We are fine, but can't leave mark on the stack
					ERR_pop_to_mark();
					
					// After the store has been updated, perform a *complete* recheck of the peer certificate, the existing context can be in mid recursion, so hands off!
					X509_STORE_CTX* vrfy_ctx = X509_STORE_CTX_new();
					
					if (vrfy_ctx && X509_STORE_CTX_init(vrfy_ctx, store, cert, X509_STORE_CTX_get_chain(ctx)))
					{
						// Welcome to recursion hell... it should at most be 2n, where n is the number of certificates in the chain
						X509_STORE_CTX_set_ex_data(vrfy_ctx, SSL_get_ex_data_X509_STORE_CTX_idx(), ssl);
						X509_STORE_CTX_set_verify_cb(vrfy_ctx, SSL_get_verify_callback(ssl));
						
						int verify_result = X509_verify_cert(vrfy_ctx);
						if (verify_result >= 0)
						{
							err = X509_STORE_CTX_get_error(vrfy_ctx);
							
							// Watch out for weird library errors that might not set the context error code
							if (err == X509_V_OK && verify_result == 0)
								err = X509_V_ERR_UNSPECIFIED;
						}
					}
					
					X509_STORE_CTX_set_error(ctx, err); // Set the current cert error to the context being verified.
					if (vrfy_ctx) X509_STORE_CTX_free(vrfy_ctx);
				}
				else ERR_pop_to_mark();
				
				// KeyPrint was not root certificate or we don't have the issuer certificate, the best we can do is trust the pinned KeyPrint
				if (err == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN || err == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY || err == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT)
				{
					X509_STORE_CTX_set_error(ctx, X509_V_OK);
					// Set this to allow ignoring any follow up errors caused by the incomplete chain
					SSL_set_ex_data(ssl, CryptoManager::idxVerifyData, &CryptoManager::trustedKeyprint);
					return 1;
				}
			}
			
			if (err == X509_V_OK)
				return 1;
				
			// We are still here, something went wrong, complain below...
			preverify_ok = 0;
		}
		else
		{
			if (X509_STORE_CTX_get_error_depth(ctx) > 0)
				return 1;
				
			// TODO: How to get this into HubFrame?
			preverify_ok = 0;
			err = X509_V_ERR_APPLICATION_VERIFICATION;
#ifndef _DEBUG
			error = "Supplied KeyPrint did not match any certificate.";
#else
			error = str(F_("Supplied KeyPrint %1% did not match %2%.") % keyp % expected_keyp);
#endif
			
			X509_STORE_CTX_set_error(ctx, err);
		}
	}
	
	// We let untrusted certificates through unconditionally, when allowed, but we like to complain regardless
	if (!preverify_ok)
	{
		if (error.empty())
			error = X509_verify_cert_error_string(err);
			
		auto fullError = formatError(ctx, error);
		if (!fullError.empty() && (!keyp.empty() || !allowUntrusted))
			LogManager::message(fullError);
	}
	
	// Don't allow untrusted connections on keyprint mismatch
	if (allowUntrusted && err != X509_V_ERR_APPLICATION_VERIFICATION)
		return 1;
		
	return preverify_ok;
}

int CryptoManager::getKeyLength(TLSTmpKeys key)
{
	switch (key)
	{
		case KEY_DH_2048:
		case KEY_RSA_2048:
			return 2048;
		case KEY_DH_4096:
			return 4096;
		default:
			dcassert(0);
			return 0;
	}
}

DH* CryptoManager::getTmpDH(int keyLen)
{
	if (keyLen < 2048)
		return nullptr;
		
	DH* tmpDH = DH_new();
	if (!tmpDH) return nullptr;
	
	// From RFC 3526; checked via http://wiki.openssl.org/index.php/Diffie-Hellman_parameters#Validating_Parameters
	switch (keyLen)
	{
		case 2048:
		{
			static unsigned char dh2048_p[] =
			{
				0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC9, 0x0F, 0xDA, 0xA2,
				0x21, 0x68, 0xC2, 0x34, 0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1,
				0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74, 0x02, 0x0B, 0xBE, 0xA6,
				0x3B, 0x13, 0x9B, 0x22, 0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
				0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B, 0x30, 0x2B, 0x0A, 0x6D,
				0xF2, 0x5F, 0x14, 0x37, 0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45,
				0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6, 0xF4, 0x4C, 0x42, 0xE9,
				0xA6, 0x37, 0xED, 0x6B, 0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED,
				0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5, 0xAE, 0x9F, 0x24, 0x11,
				0x7C, 0x4B, 0x1F, 0xE6, 0x49, 0x28, 0x66, 0x51, 0xEC, 0xE4, 0x5B, 0x3D,
				0xC2, 0x00, 0x7C, 0xB8, 0xA1, 0x63, 0xBF, 0x05, 0x98, 0xDA, 0x48, 0x36,
				0x1C, 0x55, 0xD3, 0x9A, 0x69, 0x16, 0x3F, 0xA8, 0xFD, 0x24, 0xCF, 0x5F,
				0x83, 0x65, 0x5D, 0x23, 0xDC, 0xA3, 0xAD, 0x96, 0x1C, 0x62, 0xF3, 0x56,
				0x20, 0x85, 0x52, 0xBB, 0x9E, 0xD5, 0x29, 0x07, 0x70, 0x96, 0x96, 0x6D,
				0x67, 0x0C, 0x35, 0x4E, 0x4A, 0xBC, 0x98, 0x04, 0xF1, 0x74, 0x6C, 0x08,
				0xCA, 0x18, 0x21, 0x7C, 0x32, 0x90, 0x5E, 0x46, 0x2E, 0x36, 0xCE, 0x3B,
				0xE3, 0x9E, 0x77, 0x2C, 0x18, 0x0E, 0x86, 0x03, 0x9B, 0x27, 0x83, 0xA2,
				0xEC, 0x07, 0xA2, 0x8F, 0xB5, 0xC5, 0x5D, 0xF0, 0x6F, 0x4C, 0x52, 0xC9,
				0xDE, 0x2B, 0xCB, 0xF6, 0x95, 0x58, 0x17, 0x18, 0x39, 0x95, 0x49, 0x7C,
				0xEA, 0x95, 0x6A, 0xE5, 0x15, 0xD2, 0x26, 0x18, 0x98, 0xFA, 0x05, 0x10,
				0x15, 0x72, 0x8E, 0x5A, 0x8A, 0xAC, 0xAA, 0x68, 0xFF, 0xFF, 0xFF, 0xFF,
				0xFF, 0xFF, 0xFF, 0xFF,
			};
			tmpDH->p = BN_bin2bn(dh2048_p, sizeof(dh2048_p), nullptr);
			break;
		}
		
		case 4096:
		{
			static unsigned char dh4096_p[] =
			{
				0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC9, 0x0F, 0xDA, 0xA2,
				0x21, 0x68, 0xC2, 0x34, 0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1,
				0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74, 0x02, 0x0B, 0xBE, 0xA6,
				0x3B, 0x13, 0x9B, 0x22, 0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
				0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B, 0x30, 0x2B, 0x0A, 0x6D,
				0xF2, 0x5F, 0x14, 0x37, 0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45,
				0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6, 0xF4, 0x4C, 0x42, 0xE9,
				0xA6, 0x37, 0xED, 0x6B, 0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED,
				0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5, 0xAE, 0x9F, 0x24, 0x11,
				0x7C, 0x4B, 0x1F, 0xE6, 0x49, 0x28, 0x66, 0x51, 0xEC, 0xE4, 0x5B, 0x3D,
				0xC2, 0x00, 0x7C, 0xB8, 0xA1, 0x63, 0xBF, 0x05, 0x98, 0xDA, 0x48, 0x36,
				0x1C, 0x55, 0xD3, 0x9A, 0x69, 0x16, 0x3F, 0xA8, 0xFD, 0x24, 0xCF, 0x5F,
				0x83, 0x65, 0x5D, 0x23, 0xDC, 0xA3, 0xAD, 0x96, 0x1C, 0x62, 0xF3, 0x56,
				0x20, 0x85, 0x52, 0xBB, 0x9E, 0xD5, 0x29, 0x07, 0x70, 0x96, 0x96, 0x6D,
				0x67, 0x0C, 0x35, 0x4E, 0x4A, 0xBC, 0x98, 0x04, 0xF1, 0x74, 0x6C, 0x08,
				0xCA, 0x18, 0x21, 0x7C, 0x32, 0x90, 0x5E, 0x46, 0x2E, 0x36, 0xCE, 0x3B,
				0xE3, 0x9E, 0x77, 0x2C, 0x18, 0x0E, 0x86, 0x03, 0x9B, 0x27, 0x83, 0xA2,
				0xEC, 0x07, 0xA2, 0x8F, 0xB5, 0xC5, 0x5D, 0xF0, 0x6F, 0x4C, 0x52, 0xC9,
				0xDE, 0x2B, 0xCB, 0xF6, 0x95, 0x58, 0x17, 0x18, 0x39, 0x95, 0x49, 0x7C,
				0xEA, 0x95, 0x6A, 0xE5, 0x15, 0xD2, 0x26, 0x18, 0x98, 0xFA, 0x05, 0x10,
				0x15, 0x72, 0x8E, 0x5A, 0x8A, 0xAA, 0xC4, 0x2D, 0xAD, 0x33, 0x17, 0x0D,
				0x04, 0x50, 0x7A, 0x33, 0xA8, 0x55, 0x21, 0xAB, 0xDF, 0x1C, 0xBA, 0x64,
				0xEC, 0xFB, 0x85, 0x04, 0x58, 0xDB, 0xEF, 0x0A, 0x8A, 0xEA, 0x71, 0x57,
				0x5D, 0x06, 0x0C, 0x7D, 0xB3, 0x97, 0x0F, 0x85, 0xA6, 0xE1, 0xE4, 0xC7,
				0xAB, 0xF5, 0xAE, 0x8C, 0xDB, 0x09, 0x33, 0xD7, 0x1E, 0x8C, 0x94, 0xE0,
				0x4A, 0x25, 0x61, 0x9D, 0xCE, 0xE3, 0xD2, 0x26, 0x1A, 0xD2, 0xEE, 0x6B,
				0xF1, 0x2F, 0xFA, 0x06, 0xD9, 0x8A, 0x08, 0x64, 0xD8, 0x76, 0x02, 0x73,
				0x3E, 0xC8, 0x6A, 0x64, 0x52, 0x1F, 0x2B, 0x18, 0x17, 0x7B, 0x20, 0x0C,
				0xBB, 0xE1, 0x17, 0x57, 0x7A, 0x61, 0x5D, 0x6C, 0x77, 0x09, 0x88, 0xC0,
				0xBA, 0xD9, 0x46, 0xE2, 0x08, 0xE2, 0x4F, 0xA0, 0x74, 0xE5, 0xAB, 0x31,
				0x43, 0xDB, 0x5B, 0xFC, 0xE0, 0xFD, 0x10, 0x8E, 0x4B, 0x82, 0xD1, 0x20,
				0xA9, 0x21, 0x08, 0x01, 0x1A, 0x72, 0x3C, 0x12, 0xA7, 0x87, 0xE6, 0xD7,
				0x88, 0x71, 0x9A, 0x10, 0xBD, 0xBA, 0x5B, 0x26, 0x99, 0xC3, 0x27, 0x18,
				0x6A, 0xF4, 0xE2, 0x3C, 0x1A, 0x94, 0x68, 0x34, 0xB6, 0x15, 0x0B, 0xDA,
				0x25, 0x83, 0xE9, 0xCA, 0x2A, 0xD4, 0x4C, 0xE8, 0xDB, 0xBB, 0xC2, 0xDB,
				0x04, 0xDE, 0x8E, 0xF9, 0x2E, 0x8E, 0xFC, 0x14, 0x1F, 0xBE, 0xCA, 0xA6,
				0x28, 0x7C, 0x59, 0x47, 0x4E, 0x6B, 0xC0, 0x5D, 0x99, 0xB2, 0x96, 0x4F,
				0xA0, 0x90, 0xC3, 0xA2, 0x23, 0x3B, 0xA1, 0x86, 0x51, 0x5B, 0xE7, 0xED,
				0x1F, 0x61, 0x29, 0x70, 0xCE, 0xE2, 0xD7, 0xAF, 0xB8, 0x1B, 0xDD, 0x76,
				0x21, 0x70, 0x48, 0x1C, 0xD0, 0x06, 0x91, 0x27, 0xD5, 0xB0, 0x5A, 0xA9,
				0x93, 0xB4, 0xEA, 0x98, 0x8D, 0x8F, 0xDD, 0xC1, 0x86, 0xFF, 0xB7, 0xDC,
				0x90, 0xA6, 0xC0, 0x8F, 0x4D, 0xF4, 0x35, 0xC9, 0x34, 0x06, 0x31, 0x99,
				0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			};
			tmpDH->p = BN_bin2bn(dh4096_p, sizeof(dh4096_p), nullptr);
			break;
		}
	}
	
	static unsigned char dh_g[] =
	{
		0x02,
	};
	
	tmpDH->g = BN_bin2bn(dh_g, sizeof(dh_g), nullptr);
	
	if (!tmpDH->p || !tmpDH->g)
	{
		DH_free(tmpDH);
		return nullptr;
	}
	else
	{
		return tmpDH;
	}
}

RSA* CryptoManager::getTmpRSA(int keyLen)
{
	if (keyLen < 2048)
		return nullptr;
		
	RSA* tmpRSA = RSA_new();
	BIGNUM* bn = BN_new();
	
	if (!bn || !BN_set_word(bn, RSA_F4) || !RSA_generate_key_ex(tmpRSA, keyLen, bn, nullptr))
	{
		if (bn) BN_free(bn);
		RSA_free(tmpRSA);
		return nullptr;
	}
	
	BN_free(bn);
	return tmpRSA;
}

bool CryptoManager::TLSOk() noexcept
{
	return BOOLSETTING(USE_TLS) && certsLoaded && !keyprint.empty();
}

void CryptoManager::generateCertificate()
{
	// Generate certificate using OpenSSL
	if (SETTING(TLS_PRIVATE_KEY_FILE).empty())
	{
		throw CryptoException("No private key file chosen");
	}
	if (SETTING(TLS_CERTIFICATE_FILE).empty())
	{
		throw CryptoException("No certificate file chosen");
	}
	
	ssl::BIGNUM bn(BN_new());
	ssl::RSA rsa(RSA_new());
	ssl::EVP_PKEY pkey(EVP_PKEY_new());
	ssl::X509_NAME nm(X509_NAME_new());
	ssl::X509 x509ss(X509_new());
	ssl::ASN1_INTEGER serial(ASN1_INTEGER_new());
	
	if (!bn || !rsa || !pkey || !serial || !nm || !x509ss)
	{
		throw CryptoException("Error creating objects for cert generation");
	}
	
	int days = 90;
	int keylength = 2048;
	
#define CHECK(n) if(!(n)) { throw CryptoException(#n); }
	
	// Generate key pair
	CHECK((BN_set_word(bn, RSA_F4)))
	CHECK((RSA_generate_key_ex(rsa, keylength, bn, NULL)))
	CHECK((EVP_PKEY_set1_RSA(pkey, rsa)))
	
	ByteVector fieldBytes;
	
	// Add common name (use cid)
	string name = ClientManager::getInstance()->getMyCID().toBase32().c_str();
	fieldBytes.assign(name.begin(), name.end());
	CHECK((X509_NAME_add_entry_by_NID(nm, NID_commonName, MBSTRING_ASC, &fieldBytes[0], fieldBytes.size(), -1, 0)))
	
	// Add an organisation
	string org = "DCPlusPlus (OSS/SelfSigned)";
	fieldBytes.assign(org.begin(), org.end());
	CHECK((X509_NAME_add_entry_by_NID(nm, NID_organizationName, MBSTRING_ASC, &fieldBytes[0], fieldBytes.size(), -1, 0)))
	
	// Generate unique serial
	CHECK((BN_pseudo_rand(bn, 64, 0, 0)))
	CHECK((BN_to_ASN1_INTEGER(bn, serial)))
	
	// Prepare self-signed cert
	CHECK((X509_set_version(x509ss, 0x02)))
	CHECK((X509_set_serialNumber(x509ss, serial)))
	CHECK((X509_set_issuer_name(x509ss, nm)))
	CHECK((X509_set_subject_name(x509ss, nm)))
	CHECK((X509_gmtime_adj(X509_get_notBefore(x509ss), 0)))
	CHECK((X509_gmtime_adj(X509_get_notAfter(x509ss), (long)60 * 60 * 24 * days)))
	CHECK((X509_set_pubkey(x509ss, pkey)))
	
	// Sign using own private key
	CHECK((X509_sign(x509ss, pkey, EVP_sha256())))
	
#undef CHECK
	
	// Write the key and cert
	{
		File::ensureDirectory(SETTING(TLS_PRIVATE_KEY_FILE));
		FILE* f = fopen(SETTING(TLS_PRIVATE_KEY_FILE).c_str(), "w");
		if (!f)
		{
			return;
		}
		PEM_write_RSAPrivateKey(f, rsa, NULL, NULL, 0, NULL, NULL);
		fclose(f);
	}
	{
		File::ensureDirectory(SETTING(TLS_CERTIFICATE_FILE));
		FILE* f = fopen(SETTING(TLS_CERTIFICATE_FILE).c_str(), "w");
		if (!f)
		{
			File::deleteFile(SETTING(TLS_PRIVATE_KEY_FILE));
			return;
		}
		PEM_write_X509(f, x509ss);
		fclose(f);
	}
}

void CryptoManager::loadCertificates() noexcept
{
	if (!BOOLSETTING(USE_TLS) || !clientContext || !clientALPNContext || !serverContext)
		return;
		
	const string& cert = SETTING(TLS_CERTIFICATE_FILE);
	const string& key = SETTING(TLS_PRIVATE_KEY_FILE);
	
	keyprint.clear();
	certsLoaded = false;
	
	if (cert.empty() || key.empty())
	{
		LogManager::message(STRING(NO_CERTIFICATE_FILE_SET));
		return;
	}
	
	if (File::getSize(cert) == -1 || File::getSize(key) == -1 || !checkCertificate())
	{
		// Try to generate them...
		try
		{
			generateCertificate();
			LogManager::message(STRING(CERTIFICATE_GENERATED));
		}
		catch (const CryptoException& e)
		{
			LogManager::message(STRING(CERTIFICATE_GENERATION_FAILED) + " " + e.getError());
			return;
		}
	}
	
	if (
	    SSL_CTX_use_certificate_file(serverContext, cert.c_str(), SSL_FILETYPE_PEM) != SSL_SUCCESS ||
	    SSL_CTX_use_certificate_file(clientALPNContext, cert.c_str(), SSL_FILETYPE_PEM) != SSL_SUCCESS ||
	    SSL_CTX_use_certificate_file(clientContext, cert.c_str(), SSL_FILETYPE_PEM) != SSL_SUCCESS
	)
	{
		LogManager::message(STRING(FAILED_TO_LOAD_CERTIFICATE));
		return;
	}
	
	if (
	    SSL_CTX_use_PrivateKey_file(serverContext, key.c_str(), SSL_FILETYPE_PEM) != SSL_SUCCESS ||
	    SSL_CTX_use_PrivateKey_file(clientALPNContext, key.c_str(), SSL_FILETYPE_PEM) != SSL_SUCCESS ||
	    SSL_CTX_use_PrivateKey_file(clientContext, key.c_str(), SSL_FILETYPE_PEM) != SSL_SUCCESS
	)
	{
		LogManager::message(STRING(FAILED_TO_LOAD_PRIVATE_KEY));
		return;
	}
	
	auto certs = File::findFiles(SETTING(TLS_TRUSTED_CERTIFICATES_PATH), "*.pem");
	auto certs2 = File::findFiles(SETTING(TLS_TRUSTED_CERTIFICATES_PATH), "*.crt");
	certs.insert(certs.end(), certs2.begin(), certs2.end());
	
	for (auto& i : certs)
	{
		if (
		    SSL_CTX_load_verify_locations(clientContext, i.c_str(), NULL) != SSL_SUCCESS ||
		    SSL_CTX_load_verify_locations(clientALPNContext, i.c_str(), NULL) != SSL_SUCCESS ||
		    SSL_CTX_load_verify_locations(serverContext, i.c_str(), NULL) != SSL_SUCCESS
		)
		{
			LogManager::message("Failed to load trusted certificate from " + i);
		}
	}
	
	loadKeyprint(cert.c_str());
	
	certsLoaded = true;
}

string CryptoManager::getNameEntryByNID(X509_NAME* name, int nid) noexcept
{
	int i = X509_NAME_get_index_by_NID(name, nid, -1);
	if (i == -1)
	{
		return Util::emptyString;
	}
	
	X509_NAME_ENTRY* entry = X509_NAME_get_entry(name, i);
	ASN1_STRING* str = X509_NAME_ENTRY_get_data(entry);
	if (!str)
	{
		return Util::emptyString;
	}
	
	unsigned char* buf = 0;
	i = ASN1_STRING_to_UTF8(&buf, str);
	if (i < 0)
	{
		return Util::emptyString;
	}
	
	std::string out((char*)buf, i);
	OPENSSL_free(buf);
	
	return out;
}

bool CryptoManager::checkCertificate() noexcept
{
	FILE* f = fopen(SETTING(TLS_CERTIFICATE_FILE).c_str(), "r");
	if (!f)
	{
		return false;
	}
	
	X509* tmpx509 = NULL;
	PEM_read_X509(f, &tmpx509, NULL, NULL);
	fclose(f);
	
	if (!tmpx509)
	{
		return false;
	}
	ssl::X509 x509(tmpx509);
	
	ASN1_INTEGER* sn = X509_get_serialNumber(x509);
	if (!sn || !ASN1_INTEGER_get(sn))
	{
		return false;
	}
	
	X509_NAME* name = X509_get_subject_name(x509);
	if (!name)
	{
		return false;
	}
	
	const string cn = getNameEntryByNID(name, NID_commonName);
	if (cn != ClientManager::getInstance()->getMyCID().toBase32())
	{
		return false;
	}
	
	ASN1_TIME* t = X509_get_notAfter(x509);
	if (t)
	{
		if (X509_cmp_current_time(t) < 0)
		{
			return false;
		}
	}
	
	return true;
}

const ByteVector& CryptoManager::getKeyprint() noexcept
{
	return keyprint;
}

void CryptoManager::loadKeyprint(const string& file)
{
	FILE* f = fopen(file.c_str(), "r");
	if (!f)
	{
		return;
	}
	
	X509* tmpx509 = NULL;
	PEM_read_X509(f, &tmpx509, NULL, NULL);
	fclose(f);
	
	if (!tmpx509)
	{
		return;
	}
	
	ssl::X509 x509(tmpx509);
	
	keyprint = X509_digest_internal(x509, EVP_sha256());
}

SSL_CTX* CryptoManager::getSSLContext(SSLContext wanted)
{
	switch (wanted)
	{
		case SSL_CLIENT:
			return clientContext;
		case SSL_CLIENT_ALPN:
			return clientALPNContext;
		case SSL_SERVER:
			return serverContext;
		default:
			return NULL;
	}
}

void CryptoManager::decodeBZ2(const uint8_t* is, unsigned int sz, string& os)
{
	bz_stream bs = { 0 };
	
	if (BZ2_bzDecompressInit(&bs, 0, 0) != BZ_OK)
		throw CryptoException(STRING(DECOMPRESSION_ERROR));
		
	// We assume that the files aren't compressed more than 2:1...if they are it'll work anyway,
	// but we'll have to do multiple passes...
	unsigned int bufsize = 2 * sz;
	boost::scoped_array<char> buf(new char[bufsize]);
	
	bs.avail_in = sz;
	bs.avail_out = bufsize;
	bs.next_in = reinterpret_cast<char*>(const_cast<uint8_t*>(is));
	bs.next_out = &buf[0];
	
	int err;
	
	os.clear();
	
	while ((err = BZ2_bzDecompress(&bs)) == BZ_OK)
	{
		if (bs.avail_in == 0 && bs.avail_out > 0)   // error: BZ_UNEXPECTED_EOF
		{
			BZ2_bzDecompressEnd(&bs);
			throw CryptoException(STRING(DECOMPRESSION_ERROR));
		}
		os.append(&buf[0], bufsize - bs.avail_out);
		bs.avail_out = bufsize;
		bs.next_out = &buf[0];
	}
	
	if (err == BZ_STREAM_END)
		os.append(&buf[0], bufsize - bs.avail_out);
		
	BZ2_bzDecompressEnd(&bs);
	
	if (err < 0)
	{
		// This was a real error
		throw CryptoException(STRING(DECOMPRESSION_ERROR));
	}
}

string CryptoManager::keySubst(const uint8_t* aKey, size_t len, size_t n)
{
	boost::scoped_array<uint8_t> temp(new uint8_t[len + n * 10]);
	
	size_t j = 0;
	
	for (size_t i = 0; i < len; i++)
	{
		if (isExtra(aKey[i]))
		{
			temp[j++] = '/';
			temp[j++] = '%';
			temp[j++] = 'D';
			temp[j++] = 'C';
			temp[j++] = 'N';
			switch (aKey[i])
			{
				case 0:
					temp[j++] = '0';
					temp[j++] = '0';
					temp[j++] = '0';
					break;
				case 5:
					temp[j++] = '0';
					temp[j++] = '0';
					temp[j++] = '5';
					break;
				case 36:
					temp[j++] = '0';
					temp[j++] = '3';
					temp[j++] = '6';
					break;
				case 96:
					temp[j++] = '0';
					temp[j++] = '9';
					temp[j++] = '6';
					break;
				case 124:
					temp[j++] = '1';
					temp[j++] = '2';
					temp[j++] = '4';
					break;
				case 126:
					temp[j++] = '1';
					temp[j++] = '2';
					temp[j++] = '6';
					break;
			}
			temp[j++] = '%';
			temp[j++] = '/';
		}
		else
		{
			temp[j++] = aKey[i];
		}
	}
	return string((const char*)&temp[0], j);
}

string CryptoManager::makeKey(const string& aLock)
{
	if (aLock.size() < 3)
		return Util::emptyString;
		
	boost::scoped_array<uint8_t> temp(new uint8_t[aLock.length()]);
	uint8_t v1;
	size_t extra = 0;
	
	v1 = (uint8_t)(aLock[0] ^ 5);
	v1 = (uint8_t)(((v1 >> 4) | (v1 << 4)) & 0xff);
	temp[0] = v1;
	
	string::size_type i;
	
	for (i = 1; i < aLock.length(); i++)
	{
		v1 = (uint8_t)(aLock[i] ^ aLock[i - 1]);
		v1 = (uint8_t)(((v1 >> 4) | (v1 << 4)) & 0xff);
		temp[i] = v1;
		if (isExtra(temp[i]))
			extra++;
	}
	
	temp[0] = (uint8_t)(temp[0] ^ temp[aLock.length() - 1]);
	
	if (isExtra(temp[0]))
	{
		extra++;
	}
	
	return keySubst(&temp[0], aLock.length(), extra);
}

DH* CryptoManager::tmp_dh_cb(SSL* /*ssl*/, int /*is_export*/, int keylength)
{
	initTmpKeyMaps(); // [+] FlylinDC++
	if (keylength < 2048)
		return (DH*)g_tmpKeysMap[KEY_DH_2048];
		
	void* tmpDH = NULL;
	switch (keylength)
	{
		case 2048:
			tmpDH = g_tmpKeysMap[KEY_DH_2048];
			break;
		case 4096:
			tmpDH = g_tmpKeysMap[KEY_DH_4096];
			break;
	}
	
	return (DH*)(tmpDH ? tmpDH : g_tmpKeysMap[KEY_DH_2048]);
}

RSA* CryptoManager::tmp_rsa_cb(SSL* /*ssl*/, int /*is_export*/, int keylength)
{
	initTmpKeyMaps(); // [+] FlylinDC++
	if (keylength < 2048)
		return (RSA*)g_tmpKeysMap[KEY_RSA_2048];
		
	void* tmpRSA = NULL;
	switch (keylength)
	{
		case 2048:
			tmpRSA = g_tmpKeysMap[KEY_RSA_2048];
			break;
	}
	
	return (RSA*)(tmpRSA ? tmpRSA : g_tmpKeysMap[KEY_RSA_2048]);
}

void CryptoManager::locking_function(int mode, int n, const char* /*file*/, int /*line*/)
{
	if (mode & CRYPTO_LOCK)
	{
		cs[n].lock();
	}
	else
	{
		cs[n].unlock();
	}
}

ByteVector CryptoManager::X509_digest_internal(::X509* x509, const ::EVP_MD* md)
{
	unsigned int n;
	unsigned char buf[EVP_MAX_MD_SIZE];
	
	if (!X509_digest(x509, md, buf, &n))
	{
		return ByteVector(); // Throw instead?
	}
	
	return ByteVector(buf, buf + n);
}

/**
 * @file
 * $Id$
*/
