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
#include "ClientManager.h"
#include "StringTokenizer.h"
#include "Download.h"
#include "LogManager.h"
#include "ConnectionManager.h"
#include "QueueManager.h"
#include "PGLoader.h"
#include "IpGuard.h"
#include "../FlyFeatures/flyServer.h"
const string UserConnection::FEATURE_MINISLOTS = "MiniSlots";
const string UserConnection::FEATURE_XML_BZLIST = "XmlBZList";
const string UserConnection::FEATURE_ADCGET = "ADCGet";
const string UserConnection::FEATURE_ZLIB_GET = "ZLIG";
const string UserConnection::FEATURE_TTHL = "TTHL";
const string UserConnection::FEATURE_TTHF = "TTHF";
const string UserConnection::FEATURE_ADC_BAS0 = "BAS0";
const string UserConnection::FEATURE_ADC_BASE = "BASE";
const string UserConnection::FEATURE_ADC_BZIP = "BZIP";
const string UserConnection::FEATURE_ADC_TIGR = "TIGR";
#ifdef SMT_ENABLE_FEATURE_BAN_MSG
const string UserConnection::FEATURE_BANMSG = "BanMsg"; // !SMT!-B
#endif

const string UserConnection::g_FILE_NOT_AVAILABLE = "File Not Available";
#if defined (PPA_INCLUDE_DOS_GUARD) || defined (IRAINMAN_DISALLOWED_BAN_MSG)
const string UserConnection::g_PLEASE_UPDATE_YOUR_CLIENT = "Please update your DC++ http://flylinkdc.com";
#endif

// We only want ConnectionManager to create this...
UserConnection::UserConnection(bool p_secure) :
	m_last_encoding(Text::g_systemCharset),
	state(STATE_UNCONNECTED),
	lastActivity(0),
	speed(0),
	m_chunkSize(0),
	socket(nullptr),
	slotType(NOSLOT)
{
	if (p_secure)
	{
		setFlag(FLAG_SECURE);
	}
}

UserConnection::~UserConnection()
{
	// dcassert(!m_download);
	// dcassert(!m_upload);
	dcassert(socket);
	if (socket)
	{
		socket->removeListeners();
		BufferedSocket::putBufferedSocket(socket);
	}
}
bool UserConnection::isIPGuard(ResourceManager::Strings p_id_string, bool p_is_download_connection)
{
	uint32_t l_ip4;
	if (IpGuard::is_block_ip(getRemoteIp(), l_ip4))
	{
		getUser()->setFlag(User::PG_P2PGUARD_BLOCK);
		QueueManager::getInstance()->removeSource(getUser(), QueueItem::Source::FLAG_REMOVED);
		return true;
	}
	bool l_is_ip_guard = false;
#ifdef PPA_INCLUDE_IPFILTER
	l_is_ip_guard = PGLoader::check(l_ip4);
	string l_p2p_guard;
	if (BOOLSETTING(ENABLE_P2P_GUARD) && p_is_download_connection == false)
	{
		l_p2p_guard = CFlylinkDBManager::getInstance()->is_p2p_guard(l_ip4);
		if (!l_p2p_guard.empty())
		{
			l_is_ip_guard = true;
			l_p2p_guard = " [P2PGuard] " + l_p2p_guard + " [http://emule-security.org]";
			const bool l_is_manual = l_p2p_guard.find("Manual block IP") != string::npos;
			if (getUser())
			{
				l_p2p_guard += "[User = " + getUser()->getLastNick() + "] [Hub:" + getHubUrl() + "] [Nick:" + ClientManager::findMyNick(getHubUrl()) + "]";
			}
			if (!l_is_manual)
			{
				CFlyServerJSON::pushError(38, "(" + getRemoteIp() + ')' + l_p2p_guard);
			}
		}
	}
	bool l_is_avdb_guard;
	string l_avdb_guard;
	if (BOOLSETTING(AVDB_BLOCK_CONNECTIONS))
	{
		const auto l_nick         = getUser()->getLastNick();
		const auto l_bytes_shared = getUser()->getBytesShared();
		l_is_avdb_guard = CFlylinkDBManager::getInstance()->is_avdb_guard(l_nick, l_bytes_shared, l_ip4);
		if (l_is_avdb_guard)
		{
			l_is_ip_guard = true;
			l_avdb_guard = " [AVDBGuard] ";
			if (getUser())
			{
				l_avdb_guard += "[User = " + getUser()->getLastNick() + "] [Hub:" + getHubUrl() + "] [Nick:" + ClientManager::findMyNick(getHubUrl()) + "]";
			}
			CFlyServerJSON::pushError(43, "(" + getRemoteIp() + ')' + l_avdb_guard);
		}
	}
	if (l_is_ip_guard)
	{
		const auto l_block_message = l_p2p_guard + l_avdb_guard;
		error(STRING(YOUR_IP_IS_BLOCKED) + l_block_message);
		if (l_p2p_guard.empty())
		{
			if (l_avdb_guard.empty())
			{
				getUser()->setFlag(User::PG_IPTRUST_BLOCK);
			}
		}
		else
		{
			getUser()->setFlag(User::PG_P2PGUARD_BLOCK);
		}
		if (!l_avdb_guard.empty())
		{
			getUser()->setFlag(User::PG_AVDB_BLOCK);
		}
		LogManager::message("IPFilter: " + ResourceManager::getString(p_id_string) + " (" + getRemoteIp() + ") " + l_block_message);
		QueueManager::getInstance()->removeSource(getUser(), QueueItem::Source::FLAG_REMOVED);
		return true;
	}
#endif
	return false;
}
void UserConnection::on(BufferedSocketListener::Line, const string& aLine) noexcept
{
	dcassert(!ClientManager::isShutdown())
	if (aLine.length() < 2 || ClientManager::isShutdown())
		return;
	COMMAND_DEBUG(aLine, DebugTask::CLIENT_IN, getRemoteIpPort());
	
	if (aLine[0] == 'C' && !isSet(FLAG_NMDC))
	{
		if (!Text::validateUtf8(aLine))
		{
			// @todo Report to user?
			return;
		}
		dispatch(aLine);
		return;
	}
	else if (aLine[0] == '$')
	{
		setFlag(FLAG_NMDC);
	}
	else
	{
		// We shouldn't be here?
		if (getUser() && aLine.length() < 255)
			ClientManager::getInstance()->setUnknownCommand(getUser(), aLine);
			
		fly_fire2(UserConnectionListener::ProtocolError(), this, "Invalid data");
		return;
	}
	
	string cmd;
	string param;
	
	string::size_type x = aLine.find(' ');
	
	if (x == string::npos)
	{
		cmd = aLine.substr(1);
	}
	else
	{
		cmd = aLine.substr(1, x - 1);
		param = aLine.substr(x + 1);
	}
	if (cmd == "FLY-TEST-PORT")
	{
		const auto l_magic = param.substr(0, 39);
		SettingsManager::g_TestTCPLevel = false;
		if (ClientManager::getMyCID().toBase32() == l_magic)
		{
			SettingsManager::g_TestTCPLevel = CFlyServerJSON::setTestPortOK(SETTING(TCP_PORT), "tcp");
			if (SettingsManager::g_TestTCPLevel == false)
			{
				LogManager::message("Test TCP port - Failed!");
			}
			else
			{
				LogManager::message("Test TCP port - OK!");
			}
		}
		else
		{
			CFlyServerJSON::pushError(57, "TCP Error magic value = " + l_magic);
		}
		if (SettingsManager::g_TestTCPLevel)
		{
			SettingsManager::set(SettingsManager::FORCE_PASSIVE_INCOMING_CONNECTIONS, 0);
#ifdef FLYLINKDC_USE_AUTOMATIC_PASSIVE_CONNECTION
			SettingsManager::set(SettingsManager::AUTO_PASSIVE_INCOMING_CONNECTIONS, 0);
#endif
		}
	}
	else if (cmd == "MyNick")
	{
		if (!param.empty())
		{
			fly_fire2(UserConnectionListener::MyNick(), this, param);
		}
	}
	else if (cmd == "Direction")
	{
		x = param.find(' ');
		if (x != string::npos)
		{
			fly_fire3(UserConnectionListener::Direction(), this, param.substr(0, x), param.substr(x + 1));
		}
	}
	else if (cmd == "Error")
	{
		if (param.compare(0, g_FILE_NOT_AVAILABLE.size(), g_FILE_NOT_AVAILABLE) == 0 ||
		        param.rfind(/*path/file*/" no more exists") != string::npos)
		{
			// [+] SSA
			if (getDownload()) // Не понятно почему падаю тут - https://drdump.com/Problem.aspx?ProblemID=96544
			{
				if (getDownload()->isSet(Download::FLAG_USER_GET_IP)) // Crash https://drdump.com/Problem.aspx?ClientID=ppa&ProblemID=90376
				{
					fly_fire1(UserConnectionListener::CheckUserIP(), this);
				}
				else
				{
					fly_fire1(UserConnectionListener::FileNotAvailable(), this);
				}
			}
		}
		else
		{
			if (param.compare(0, 7, "CTM2HUB", 7) == 0)
			{
				// https://github.com/Verlihub/verlihub-1.0.0/blob/4f5ad13b5aa6d5a3c2ec94262f7b7bf1b90fc567/src/cdcproto.cpp#L2358
				ConnectionManager::addCTM2HUB(getServerPort(), getHintedUser());
			}
			dcdebug("Unknown $Error %s\n", param.c_str());
			fly_fire2(UserConnectionListener::ProtocolError(), this, param);
		}
	}
	else if (cmd == "Get")
	{
		x = param.find('$');
		if (x != string::npos)
		{
			fly_fire3(UserConnectionListener::Get(), this, Text::toUtf8(param.substr(0, x), m_last_encoding), Util::toInt64(param.substr(x + 1)) - (int64_t)1);
		}
	}
	else if (cmd == "Key")
	{
		if (!param.empty())
		{
			const string l_ip = getRemoteIp();
			uint32_t l_ip4;
			if (!IpGuard::is_block_ip(l_ip, l_ip4))
			{
				fly_fire2(UserConnectionListener::Key(), this, param);
			}
			else
			{
				dcdebug("Block IP %s", l_ip.c_str());
			}
		}
	}
	else if (cmd == "Lock")
	{
		if (!param.empty())
		{
			x = param.find(' ');
			fly_fire2(UserConnectionListener::CLock(), this, (x != string::npos) ? param.substr(0, x) : param);
		}
	}
	else if (cmd == "Send")
	{
		fly_fire1(UserConnectionListener::Send(), this);
	}
	else if (cmd == "MaxedOut")
	{
		fly_fire2(UserConnectionListener::MaxedOut(), this, param);
	}
	else if (cmd == "Supports")
	{
		if (!param.empty())
		{
			fly_fire2(UserConnectionListener::Supports(), this, StringTokenizer<string>(param, ' ').getTokensForWrite()); // [!] IRainman fix: http://code.google.com/p/flylinkdc/issues/detail?id=1112
		}
	}
	else if (cmd.compare(0, 3, "ADC", 3) == 0)
	{
		dispatch(aLine, true);
	}
	else if (cmd == "ListLen")
	{
		if (!param.empty())
		{
			fly_fire2(UserConnectionListener::ListLength(), this, param);
		}
	}
	else if (cmd == "GetListLen")
	{
		fly_fire1(UserConnectionListener::GetListLength(), this);
	}
	else
	{
		if (getUser() && aLine.length() < 255)
			ClientManager::getInstance()->setUnknownCommand(getUser(), aLine);
			
		dcdebug("UserConnection Unknown NMDC command: %.50s\n", aLine.c_str());
#ifdef FLYLINKDC_BETA
		string l_log = "UserConnection:: Unknown NMDC command: = " + aLine + " hub = " + getHubUrl() + " remote IP = " + getRemoteIpPort();
		if (getHintedUser().user)
		{
			l_log += " Nick = " + getHintedUser().user->getLastNick();
		}
		LogManager::message(l_log);
#endif
		unsetFlag(FLAG_NMDC);
	}
}

void UserConnection::connect(const string& aServer, uint16_t aPort, uint16_t localPort, BufferedSocket::NatRoles natRole)
{
	dcassert(!socket);
	
	socket = BufferedSocket::getBufferedSocket(0);
	socket->addListener(this);
	socket->connect(aServer, aPort, localPort, natRole, isSet(FLAG_SECURE), BOOLSETTING(ALLOW_UNTRUSTED_CLIENTS), true);
}

void UserConnection::accept(const Socket& aServer)
{
	dcassert(!socket);
	socket = BufferedSocket::getBufferedSocket(0);
	socket->addListener(this);
	const bool bAllowUntrusred = BOOLSETTING(ALLOW_UNTRUSTED_CLIENTS);
	setPort(socket->accept(aServer, isSet(FLAG_SECURE), bAllowUntrusred));
}

void UserConnection::inf(bool withToken)
{
	AdcCommand c(AdcCommand::CMD_INF);
	c.addParam("ID", ClientManager::getMyCID().toBase32());
	if (withToken)
	{
		c.addParam("TO", getUserConnectionToken());
	}
	send(c);
}

void UserConnection::sup(const StringList& features)
{
	AdcCommand c(AdcCommand::CMD_SUP);
	for (auto i = features.cbegin(); i != features.cend(); ++i)
	{
		c.addParam(*i);
	}
	send(c);
}

void UserConnection::supports(const StringList& feat)
{
	const string x = Util::toSupportsCommand(feat);
	send(x);
}

void UserConnection::handle(AdcCommand::STA t, const AdcCommand& c)
{
	if (c.getParameters().size() >= 2)
	{
		const string& code = c.getParam(0);
		if (!code.empty() && code[0] - '0' == AdcCommand::SEV_FATAL)
		{
			fly_fire2(UserConnectionListener::ProtocolError(), this, c.getParam(1));
			return;
		}
	}
	
	fly_fire2(t, this, c);
}

void UserConnection::on(Connected) noexcept
{
	setLastActivity();
	fly_fire1(UserConnectionListener::Connected(), this);
}
void UserConnection::on(Data, uint8_t* p_data, size_t p_len) noexcept
{
	setLastActivity();
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
	if (p_len && BOOLSETTING(ENABLE_RATIO_USER_LIST))
		getUser()->AddRatioDownload(getSocket()->getIp4(), p_len);
#endif
	fly_fire3(UserConnectionListener::Data(), this, p_data, p_len);
}

void UserConnection::on(BytesSent, size_t p_Bytes, size_t p_Actual) noexcept
{
	setLastActivity();
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
	if (p_Actual && BOOLSETTING(ENABLE_RATIO_USER_LIST))
		getUser()->AddRatioUpload(getSocket()->getIp4(), p_Actual);
#endif
	fly_fire3(UserConnectionListener::BytesSent(), this, p_Bytes, p_Actual);
}

#ifdef FLYLINKDC_USE_CROOKED_HTTP_CONNECTION
void UserConnection::on(ModeChange) noexcept
{
	setLastActivity();
	fly_fire1(UserConnectionListener::ModeChange(), this);
}
#endif

void UserConnection::on(TransmitDone) noexcept
{
	fly_fire1(UserConnectionListener::TransmitDone(), this);
}

void UserConnection::on(Updated) noexcept
{
	fly_fire1(UserConnectionListener::Updated(), this);
}

void UserConnection::on(Failed, const string& aLine) noexcept
{
	setState(STATE_UNCONNECTED);
	if (getUser()) // fix crash https://www.crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=44660
	{
		getUser()->fixLastIP();
	}
	fly_fire2(UserConnectionListener::Failed(), this, aLine);
	delete this;
}

// # ms we should aim for per segment
static const int64_t SEGMENT_TIME = 120 * 1000;
static const int64_t MIN_CHUNK_SIZE = 64 * 1024;

void UserConnection::updateChunkSize(int64_t leafSize, int64_t lastChunk, uint64_t ticks)
{

	if (m_chunkSize == 0)
	{
		m_chunkSize = std::max((int64_t)64 * 1024, std::min(lastChunk, (int64_t)1024 * 1024));
		return;
	}
	
	if (ticks <= 10)
	{
		// Can't rely on such fast transfers - double
		m_chunkSize *= 2;
		return;
	}
	
	double lastSpeed = (1000. * lastChunk) / ticks;
	
	int64_t targetSize = m_chunkSize;
	
	// How long current chunk size would take with the last speed...
	double msecs = 1000 * double(targetSize) / lastSpeed;
	
	if (msecs < SEGMENT_TIME / 4)
	{
		targetSize *= 2;
	}
	else if (msecs < SEGMENT_TIME / 1.25)
	{
		targetSize += leafSize;
	}
	else if (msecs < SEGMENT_TIME * 1.25)
	{
		// We're close to our target size - don't change it
	}
	else if (msecs < SEGMENT_TIME * 4)
	{
		targetSize = std::max(MIN_CHUNK_SIZE, targetSize - m_chunkSize);
	}
	else
	{
		targetSize = std::max(MIN_CHUNK_SIZE, targetSize / 2);
	}
	
	m_chunkSize = targetSize;
}

void UserConnection::send(const string& aString)
{
	setLastActivity();
	COMMAND_DEBUG(aString, DebugTask::CLIENT_OUT, getRemoteIpPort());
	socket->write(aString);
}

// !SMT!-S
void UserConnection::setUser(const UserPtr& aUser)
{
	m_hintedUser.user = aUser;
	if (!socket)
		return;
		
	if (!aUser)
	{
		setUploadLimit(FavoriteUser::UL_NONE);
	}
	else
	{
		int limit;
		FavoriteUser::MaskType l_flags;
		if (FavoriteManager::getFavUserParam(aUser, l_flags, limit))
		{
			setUploadLimit(limit);
		}
	}
}

void UserConnection::setUploadLimit(int lim)
{
	switch (lim)
	{
		case FavoriteUser::UL_BAN:
			disconnect(true);
			break;
		case FavoriteUser::UL_SU:
			socket->setMaxSpeed(-1);
			break;
		case FavoriteUser::UL_NONE:
			socket->setMaxSpeed(0);
			break;
		default:
			socket->setMaxSpeed(lim * 1024);
	}
}

void UserConnection::maxedOut(size_t queue_position)
{
	if (isSet(FLAG_NMDC))
	{
		send("$MaxedOut " + Util::toString(queue_position) + '|');
	}
	else
	{
		AdcCommand cmd(AdcCommand::SEV_RECOVERABLE, AdcCommand::ERROR_SLOTS_FULL, "Slots full");
		cmd.addParam("QP", Util::toString(queue_position));
		send(cmd);
	}
}


void UserConnection::fileNotAvail(const std::string& msg /*= g_FILE_NOT_AVAILABLE*/)
{
	isSet(FLAG_NMDC) ? send("$Error " + msg + '|') : send(AdcCommand(AdcCommand::SEV_RECOVERABLE, AdcCommand::ERROR_FILE_NOT_AVAILABLE, msg));
}

/**
 * @file
 * $Id: UserConnection.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
