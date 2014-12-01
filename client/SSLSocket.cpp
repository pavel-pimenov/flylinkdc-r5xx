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

#include "stdinc.h"


#include "SSLSocket.h"
#include "LogManager.h"
#include "SettingsManager.h"
#include "ResourceManager.h"

#ifdef HEADER_OPENSSLV_H
# include <openssl/err.h>
#endif

SSLSocket::SSLSocket(SSL_CTX* context) : ctx(context), ssl(0)
#ifndef HEADER_OPENSSLV_H
	, finished(false)
#endif

{

}

void SSLSocket::connect(const string& aIp, uint16_t aPort)
{
	Socket::connect(aIp, aPort);
	
	waitConnected(0);
}

bool SSLSocket::waitConnected(uint64_t millis)
{
	if (!ssl)
	{
		if (!Socket::waitConnected(millis))
		{
			return false;
		}
		ssl.reset(SSL_new(ctx));
		if (!ssl)
			checkSSL(-1);
			
		checkSSL(SSL_set_fd(ssl, m_sock));
	}
	
	if (SSL_is_init_finished(ssl))
	{
		return true;
	}
	
	while (true)
	{
		// OpenSSL needs server handshake for NAT traversal
		int ret = ssl->server ? SSL_accept(ssl) : SSL_connect(ssl);
		if (ret == 1)
		{
			dcdebug("Connected to SSL server using %s\n", SSL_get_cipher(ssl));
#ifndef HEADER_OPENSSLV_H
			finished = true;
#endif
			return true;
		}
		if (!waitWant(ret, millis))
		{
			return false;
		}
	}
}

uint16_t SSLSocket::accept(const Socket& listeningSocket)
{
	auto ret = Socket::accept(listeningSocket);
	waitAccepted(0);
	return ret;
}

bool SSLSocket::waitAccepted(uint64_t millis)
{
	if (!ssl)
	{
		if (!Socket::waitAccepted(millis))
		{
			return false;
		}
		ssl.reset(SSL_new(ctx));
		if (!ssl)
			checkSSL(-1);
			
		checkSSL(SSL_set_fd(ssl, m_sock));
	}
	
	if (SSL_is_init_finished(ssl))
	{
		return true;
	}
	
	while (true)
	{
		int ret = SSL_accept(ssl);
		if (ret == 1)
		{
			dcdebug("Connected to SSL client using %s\n", SSL_get_cipher(ssl));
#ifndef HEADER_OPENSSLV_H
			finished = true;
#endif
			return true;
		}
		if (!waitWant(ret, millis))
		{
			return false;
		}
	}
}

bool SSLSocket::waitWant(int ret, uint64_t millis)
{
#ifdef HEADER_OPENSSLV_H
	int err = SSL_get_error(ssl, ret);
	switch (err)
	{
		case SSL_ERROR_WANT_READ:
			return wait(millis, Socket::WAIT_READ) == WAIT_READ;
		case SSL_ERROR_WANT_WRITE:
			return wait(millis, Socket::WAIT_WRITE) == WAIT_WRITE;
#else
	int err = ssl->last_error;
	switch (err)
	{
		case GNUTLS_E_INTERRUPTED:
		case GNUTLS_E_AGAIN:
		{
			int waitFor = wait(millis, Socket::WAIT_READ | Socket::WAIT_WRITE);
			return (waitFor & Socket::WAIT_READ) || (waitFor & Socket::WAIT_WRITE);
		}
#endif
		// Check if this is a fatal error...
		default:
			checkSSL(ret);
	}
	dcdebug("SSL: Unexpected fallthrough");
	// There was no error?
	return true;
}

int SSLSocket::read(void* aBuffer, int aBufLen)
{
	if (!ssl)
	{
		return -1;
	}
	int len = checkSSL(SSL_read(ssl, aBuffer, aBufLen));
	
	if (len > 0)
	{
		g_stats.m_ssl.totalDown += len;
		//dcdebug("In(s): %.*s\n", len, (char*)aBuffer);
	}
	return len;
}

int SSLSocket::write(const void* aBuffer, int aLen)
{
	if (!ssl)
	{
		return -1;
	}
	int ret = checkSSL(SSL_write(ssl, aBuffer, aLen));
	if (ret > 0)
	{
		g_stats.m_ssl.totalUp += ret;
		//dcdebug("Out(s): %.*s\n", ret, (char*)aBuffer);
	}
	return ret;
}

int SSLSocket::checkSSL(int ret)
{
	if (!ssl)
	{
		return -1;
	}
	if (ret <= 0)
	{
		/* inspired by boost.asio (asio/ssl/detail/impl/engine.ipp, function engine::perform) and
		the SSL_get_error doc at <https://www.openssl.org/docs/ssl/SSL_get_error.html>. */
		auto err = SSL_get_error(ssl, ret);
		switch (err)
		{
			case SSL_ERROR_NONE:        // Fallthrough - YaSSL doesn't for example return an openssl compatible error on recv fail
			case SSL_ERROR_WANT_READ:   // Fallthrough
			case SSL_ERROR_WANT_WRITE:
				return -1;
			case SSL_ERROR_ZERO_RETURN:
#ifndef HEADER_OPENSSLV_H
				if (ssl->last_error == GNUTLS_E_INTERRUPTED || ssl->last_error == GNUTLS_E_AGAIN)
					return -1;
#endif
				throw SocketException(STRING(CONNECTION_CLOSED));
			case SSL_ERROR_SYSCALL:
			{
				auto sys_err = ERR_get_error();
				if (sys_err == 0)
				{
					if (ret == 0)
					{
						dcdebug("TLS error: call ret = %d, SSL_get_error = %d, ERR_get_error = %d\n", ret, err, sys_err);
						throw SSLSocketException(STRING(CONNECTION_CLOSED));
					}
					sys_err = ::GetLastError();
				}
				throw SSLSocketException(sys_err);
			}
			default:
				/* don't bother getting error messages from the codes because 1) there is some
				additional management necessary (eg SSL_load_error_strings) and 2) openssl error codes
				aren't shown to the end user; they only hit standard output in debug builds. */
				dcdebug("TLS error: call ret = %d, SSL_get_error = %d, ERR_get_error = %d\n", ret, err, ERR_get_error());
				throw SSLSocketException(STRING(TLS_ERROR));
		}
	}
	return ret;
}

int SSLSocket::wait(uint64_t millis, int waitFor)
{
#ifdef HEADER_OPENSSLV_H
	if (ssl && (waitFor & Socket::WAIT_READ))
	{
		/** @todo Take writing into account as well if reading is possible? */
		char c;
		if (SSL_peek(ssl, &c, 1) > 0)
			return WAIT_READ;
	}
#endif
	return Socket::wait(millis, waitFor);
}

bool SSLSocket::isTrusted() const noexcept
{
    if (!ssl)
{
return false;
}

#ifdef HEADER_OPENSSLV_H
if (SSL_get_verify_result(ssl) != X509_V_OK)
{
return false;
}
#else
if (gnutls_certificate_verify_peers(((SSL*)ssl)->gnutls_state) != 0)
{
return false;
}
#endif

X509* cert = SSL_get_peer_certificate(ssl);
if (!cert)
{
return false;
}

X509_free(cert);

return true;
}

string SSLSocket::getCipherName() const noexcept
{
    if (!ssl)
    return Util::emptyString;
    
    return SSL_get_cipher_name(ssl);
}

vector<uint8_t> SSLSocket::getKeyprint() const noexcept
	{
#ifdef HEADER_OPENSSLV_H
	
	    if (!ssl)
	    return Util::emptyByteVector; // [!] IRainman opt.
	    X509* x509 = SSL_get_peer_certificate(ssl);
	    if (!x509)
		    return Util::emptyByteVector; // [!] IRainman opt.
		    
		    return ssl::X509_digest(x509, EVP_sha256());
#else
	    return Util::emptyByteVector; // [!] IRainman opt.
#endif
		}
		
		void SSLSocket::shutdown() noexcept
		{
			if (ssl)
				SSL_shutdown(ssl);
		}

void SSLSocket::close() noexcept
{
	if (ssl)
	{
		ssl.reset();
	}
	Socket::shutdown();
	Socket::close();
}

