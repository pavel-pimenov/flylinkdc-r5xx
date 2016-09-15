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

#ifndef DCPLUSPLUS_DCPP_USER_CONNECTION_H
#define DCPLUSPLUS_DCPP_USER_CONNECTION_H

#include "UserConnectionListener.h"
#include "ClientManager.h"
#include "CFlylinkDBManager.h"
#include "FavoriteUser.h"

#include "Upload.h"

class UserConnection : public Speaker<UserConnectionListener>,
	private BufferedSocketListener, public Flags, private CommandHandler<UserConnection>
#ifdef _DEBUG
	, private boost::noncopyable
#endif
{
	public:
		friend class ConnectionManager;
		
		static const string FEATURE_MINISLOTS;
		static const string FEATURE_XML_BZLIST;
		static const string FEATURE_ADCGET;
		static const string FEATURE_ZLIB_GET;
		static const string FEATURE_TTHL;
		static const string FEATURE_TTHF;
		static const string FEATURE_ADC_BAS0;
		static const string FEATURE_ADC_BASE;
		static const string FEATURE_ADC_BZIP;
		static const string FEATURE_ADC_TIGR;
#ifdef SMT_ENABLE_FEATURE_BAN_MSG
		static const string FEATURE_BANMSG; // !SMT!-B
#endif
		
		static const string g_FILE_NOT_AVAILABLE;
#if defined (PPA_INCLUDE_DOS_GUARD) || defined (IRAINMAN_DISALLOWED_BAN_MSG)
		static const string g_PLEASE_UPDATE_YOUR_CLIENT;
#endif
		
		enum KnownSupports // [!] IRainman fix
		{
			FLAG_SUPPORTS_MINISLOTS     = 1,
			FLAG_SUPPORTS_XML_BZLIST    = 1 << 1,
			FLAG_SUPPORTS_ADCGET        = 1 << 2,
			FLAG_SUPPORTS_ZLIB_GET      = 1 << 3,
			FLAG_SUPPORTS_TTHL          = 1 << 4,
			FLAG_SUPPORTS_TTHF          = 1 << 5,
			FLAG_SUPPORTS_BANMSG        = 1 << 6, // !SMT!-S
			FLAG_SUPPORTS_LAST = FLAG_SUPPORTS_BANMSG
		};
		
		enum Flags // [!] IRainman fix
		{
			FLAG_INTERNAL_FIRST = FLAG_SUPPORTS_LAST,
			FLAG_NMDC                   = FLAG_INTERNAL_FIRST << 1,
#ifdef IRAINMAN_ENABLE_OP_VIP_MODE
			FLAG_OP                     = FLAG_INTERNAL_FIRST << 2,
#endif
			FLAG_UPLOAD                 = FLAG_INTERNAL_FIRST << 3, //-V112
			FLAG_DOWNLOAD               = FLAG_INTERNAL_FIRST << 4,
			FLAG_INCOMING               = FLAG_INTERNAL_FIRST << 5,
			FLAG_ASSOCIATED             = FLAG_INTERNAL_FIRST << 6, //-V112
			FLAG_SECURE                 = FLAG_INTERNAL_FIRST << 7
		};
		
		enum States
		{
			// ConnectionManager
			STATE_UNCONNECTED,
			STATE_CONNECT,
			
			// Handshake
			STATE_SUPNICK,      // ADC: SUP, Nmdc: $Nick
			STATE_INF,
			STATE_LOCK,
			STATE_DIRECTION,
			STATE_KEY,
			
			// UploadManager
			STATE_GET,          // Waiting for GET
			STATE_SEND,         // Waiting for $Send
			
			// DownloadManager
			STATE_SND,  // Waiting for SND
			STATE_IDLE, // No more downloads for the moment
			
			// Up & down
			STATE_RUNNING,      // Transmitting data
			
		};
		
		enum SlotTypes
		{
			NOSLOT      = 0,
			STDSLOT     = 1,
			EXTRASLOT   = 2,
			PARTIALSLOT = 3
		};
		
		short getNumber() const
		{
			return (short)((((size_t)this) >> 2) & 0x7fff);
		}
		bool isIPGuard(ResourceManager::Strings p_id_string, bool p_is_download_connection);
		// NMDC stuff
		void myNick(const string& aNick)
		{
//#ifdef IRAINMAN_USE_UNICODE_IN_NMDC
//			send("$MyNick " + aNick + '|');
//#else
			send("$MyNick " + Text::fromUtf8(aNick, m_last_encoding) + '|');
//#endif
		}
		void lock(const string& aLock, const string& aPk)
		{
			send("$Lock " + aLock + " Pk=" + aPk + '|');
		}
		void key(const string& aKey)
		{
			send("$Key " + aKey + '|');
		}
		void direction(const string& aDirection, int aNumber)
		{
			send("$Direction " + aDirection + ' ' + Util::toString(aNumber) + '|');
		}
		void fileLength(const string& aLength)
		{
			send("$FileLength " + aLength + '|');
		}
		void error(const string& aError)
		{
			send("$Error " + aError + '|');
		}
		void listLen(const string& aLength)
		{
			send("$ListLen " + aLength + '|');
		}
		
		void maxedOut(size_t queue_position);
		void fileNotAvail(const std::string& msg = g_FILE_NOT_AVAILABLE);
		void supports(const StringList& feat);
		
		// ADC Stuff
		void sup(const StringList& features);
		void inf(bool withToken);
		/*      void get(const string& aType, const string& aName, const int64_t aStart, const int64_t aBytes)
		        {
		            send(AdcCommand(AdcCommand::CMD_GET).addParam(aType).addParam(aName).addParam(Util::toString(aStart)).addParam(Util::toString(aBytes)));
		        }
		        void snd(const string& aType, const string& aName, const int64_t aStart, const int64_t aBytes)
		        {
		            send(AdcCommand(AdcCommand::CMD_SND).addParam(aType).addParam(aName).addParam(Util::toString(aStart)).addParam(Util::toString(aBytes)));
		        }
		        */
		void send(const AdcCommand& c)
		{
			send(c.toString(0, isSet(FLAG_NMDC)));
		}
		
		void setDataMode(int64_t aBytes = -1)
		{
			dcassert(socket);
			if (socket)
				socket->setDataMode(aBytes);
		}
		void setLineMode(size_t rollback)
		{
			dcassert(socket);
			if (socket)
				socket->setLineMode(rollback);
		}
		
		void connect(const string& aServer, uint16_t aPort, uint16_t localPort, const BufferedSocket::NatRoles natRole);
		void accept(const Socket& aServer);
		
		void updated()
		{
			dcassert(socket); // [+] IRainman fix.
			if (socket)
				socket->updated();
		}
		
		void disconnect(bool graceless = false)
		{
			//dcassert(socket); // [+] IRainman fix.
			if (socket)
				socket->disconnect(graceless);
		}
		void transmitFile(InputStream* f)
		{
			dcassert(socket); // [+] IRainman fix.
			if (socket)
				socket->transmitFile(f);
		}
		
		const string& getDirectionString() const
		{
			static const string g_UPLOAD   = "Upload";
			static const string g_DOWNLOAD = "Download";
			dcassert(isSet(FLAG_UPLOAD) ^ isSet(FLAG_DOWNLOAD));
			return isSet(FLAG_UPLOAD) ? g_UPLOAD : g_DOWNLOAD;
		}
		
		const UserPtr& getUser() const
		{
			return m_hintedUser.user; // [!] IRainman add HintedUser
		}
		const UserPtr& getUser()
		{
			return m_hintedUser.user; // [!] IRainman add HintedUser
		}
		const HintedUser& getHintedUser() const
		{
			return m_hintedUser; // [!] IRainman add HintedUser
		}
		const HintedUser& getHintedUser()
		{
			return m_hintedUser; // [!] IRainman add HintedUser
		}
		
		bool isSecure() const
		{
			dcassert(socket); // [+] IRainman fix.
			return socket && socket->isSecure();
		}
		bool isTrusted() const
		{
			dcassert(socket); // [+] IRainman fix.
			return socket && socket->isTrusted();
		}
		string getCipherName() const noexcept
		{
		    dcassert(socket); // [+] IRainman fix.
		    return socket ? socket->getCipherName() : Util::emptyString;
		}
		vector<uint8_t> getKeyprint() const
		{
			dcassert(socket); // [+] IRainman fix.
			return socket ? socket->getKeyprint() : Util::emptyByteVector; // [!] IRainman opt.
		}
		
		string getRemoteIpPort() const
		{
			dcassert(socket);
			return socket ? socket->getIp() + ':' + Util::toString(socket->getPort()) : Util::emptyString;
		}
		
		string getRemoteIp() const
		{
			dcassert(socket); // [+] IRainman fix.
			return socket ? socket->getIp() : Util::emptyString;
		}
		DownloadPtr& getDownload()
		{
			dcassert(isSet(FLAG_DOWNLOAD));
			return m_download;
		}
		uint16_t getPort() const
		{
			dcassert(socket); // [+] IRainman fix.
			return socket ? socket->getPort() : 0;
		}
		void setPort(uint16_t p_port) const
		{
			dcassert(socket); // [+] IRainman fix.
			if (socket)
				socket->setPort(p_port);
		}
		void setDownload(const DownloadPtr& d)
		{
			dcassert(isSet(FLAG_DOWNLOAD));
			m_download = d;
		}
		UploadPtr& getUpload()
		{
			dcassert(isSet(FLAG_UPLOAD));
			return m_upload;
		}
		void setUpload(const UploadPtr& aUpload)
		{
			dcassert(isSet(FLAG_UPLOAD));
			m_upload = aUpload;
		}
		
		void handle(AdcCommand::SUP t, const AdcCommand& c)
		{
			fly_fire2(t, this, c);
		}
		void handle(AdcCommand::INF t, const AdcCommand& c)
		{
			fly_fire2(t, this, c);
		}
		void handle(AdcCommand::GET t, const AdcCommand& c)
		{
			fly_fire2(t, this, c);
		}
		void handle(AdcCommand::SND t, const AdcCommand& c)
		{
			fly_fire2(t, this, c);
		}
		void handle(AdcCommand::STA t, const AdcCommand& c);
		void handle(AdcCommand::RES t, const AdcCommand& c)
		{
			fly_fire2(t, this, c);
		}
		void handle(AdcCommand::GFI t, const AdcCommand& c)
		{
			fly_fire2(t, this, c);
		}
		
		// Ignore any other ADC commands for now
		template<typename T> void handle(T , const AdcCommand&) { }
		
		int64_t getChunkSize() const
		{
			return m_chunkSize;
		}
		void updateChunkSize(int64_t leafSize, int64_t lastChunk, uint64_t ticks);
		
		// [!] IRainman add HintedUser
		void setHubUrl(const string& p_HubUrl)
		{
#ifdef _DEBUG
			if (p_HubUrl != "DHT")
				dcassert(p_HubUrl == Text::toLower(p_HubUrl));
#endif
			m_hintedUser.hint = p_HubUrl;
		}
		const string& getHubUrl()
		{
			return m_hintedUser.hint;
		}
		// [~] IRainman
		
		GETSET(string, m_user_connection_token, UserConnectionToken);
		//GETSET(string, m_connection_token, ConnectionToken);
		GETSET(int64_t, speed, Speed);
		
		uint64_t m_lastActivity;
		unsigned m_count_activite;
		void setLastActivity()
		{
			m_count_activite++;
		}
		uint64_t getLastActivity()
		{
			if (m_count_activite)
			{
				m_lastActivity = GET_TICK();
				m_count_activite = 0;
			}
			return m_lastActivity;
		}
	public:
		GETSET(string, m_last_encoding, Encoding);
		GETSET(States, state, State);
		GETSET(SlotTypes, slotType, SlotType);
		GETSET(string, m_server_port, ServerPort); // CTM2HUB
		BufferedSocket const* getSocket() const
		{
			return socket;
		}
		void fireBytesSent(size_t p_Bytes, size_t p_Actual);
		void fireData(uint8_t* p_data, size_t p_len);
		
	private:
		int64_t m_chunkSize;
		BufferedSocket* socket;
		HintedUser m_hintedUser; //UserPtr user; [!] IRainman add HintedUser
		
		DownloadPtr m_download;
		UploadPtr m_upload;
		
		// We only want ConnectionManager to create this...
		explicit UserConnection(bool p_secure);
		virtual ~UserConnection();
		
		friend struct DeleteFunction;
		
		void setUser(const UserPtr& aUser);
		void onLine(const string& aLine);
		void send(const string& aString);
		void setUploadLimit(int lim); // !SMT!-S
		
		void on(Connected) noexcept override;
		void on(Line, const string&) noexcept override;
		//void on(Data, uint8_t* data, size_t p_len) noexcept override;
		//void on(BytesSent, size_t p_Bytes, size_t p_Actual) noexcept override;
#ifdef FLYLINKDC_USE_CROOKED_HTTP_CONNECTION
		void on(ModeChange) noexcept override;
#endif
		void on(TransmitDone) noexcept override;
		void on(Failed, const string&) noexcept override;
		void on(Updated) noexcept override;
};

class UcSupports // [+] IRainman fix.
{
	public:
		static StringList setSupports(UserConnection* p_conn, const StringList& feat, uint8_t& knownUcSupports)
		{
			StringList unknownSupports;
			for (auto i = feat.cbegin(); i != feat.cend(); ++i)
			{
			
#define CHECK_FEAT(feature) if (*i == UserConnection::FEATURE_##feature) { p_conn->setFlag(UserConnection::FLAG_SUPPORTS_##feature); knownUcSupports |= UserConnection::FLAG_SUPPORTS_##feature; }
			
				CHECK_FEAT(MINISLOTS)
				else CHECK_FEAT(XML_BZLIST)
					else CHECK_FEAT(ADCGET)
						else CHECK_FEAT(ZLIB_GET)
							else CHECK_FEAT(TTHL)
								else CHECK_FEAT(TTHF)
#ifdef SMT_ENABLE_FEATURE_BAN_MSG
									else CHECK_FEAT(BANMSG) // !SMT!-S
#endif
										else
										{
											unknownSupports.push_back(*i);
										}
										
#undef CHECK_FEAT
										
			}
			return unknownSupports;
		}
		
		static string getSupports(const Identity& id)
		{
			string tmp;
			const auto su = id.getKnownUcSupports();
			
#define CHECK_FEAT(feature) if (su & UserConnection::FLAG_SUPPORTS_##feature) { tmp += UserConnection::FEATURE_##feature + ' '; }
			
			CHECK_FEAT(MINISLOTS);
			CHECK_FEAT(XML_BZLIST);
			CHECK_FEAT(ADCGET);
			CHECK_FEAT(ZLIB_GET);
			CHECK_FEAT(TTHL);
			CHECK_FEAT(TTHF);
#ifdef SMT_ENABLE_FEATURE_BAN_MSG
			CHECK_FEAT(BANMSG); // !SMT!-S
#endif
			
#undef CHECK_FEAT
			
			return tmp;
		}
};


#endif // !defined(USER_CONNECTION_H)

/**
 * @file
 * $Id: UserConnection.h 578 2011-10-04 14:27:51Z bigmuscle $
 */
