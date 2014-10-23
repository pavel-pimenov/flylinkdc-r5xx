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

const string UserConnection::FILE_NOT_AVAILABLE = "File Not Available";
#if defined (PPA_INCLUDE_DOS_GUARD) || defined (IRAINMAN_DISALLOWED_BAN_MSG)
const string UserConnection::PLEASE_UPDATE_YOUR_CLIENT = "Please update your DC++ http://flylinkdc.com";
#endif

void UserConnection::on(BufferedSocketListener::Line, const string& aLine) noexcept
{

	if (aLine.length() < 2)
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
			
		fire(UserConnectionListener::ProtocolError(), this, "Invalid data");  // TODO: translate
		return;
	}
	
	string cmd;
	string param;
	
	string::size_type x;
	
	if ((x = aLine.find(' ')) == string::npos)
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
		SettingsManager::g_TestTCPLevel = ClientManager::getMyCID().toBase32() == l_magic;
		if (!SettingsManager::g_TestTCPLevel)
		{
			LogManager::getInstance()->message("Error magic value = " + l_magic);
		}
	}
	else if (cmd == "MyNick")
	{
		if (!param.empty())
			fire(UserConnectionListener::MyNick(), this, param);
	}
	else if (cmd == "Direction")
	{
		x = param.find(' ');
		if (x != string::npos)
		{
			fire(UserConnectionListener::Direction(), this, param.substr(0, x), param.substr(x + 1));
		}
	}
	else if (cmd == "Error")
	{
		if (param.compare(0, FILE_NOT_AVAILABLE.size(), FILE_NOT_AVAILABLE) == 0 ||
		        param.rfind(/*path/file*/" no more exists") != string::npos)
		{
			// [+] SSA
			if (getDownload()->isSet(Download::FLAG_USER_GET_IP)) // Crash https://drdump.com/Problem.aspx?ClientID=ppa&ProblemID=90376
			{
				fire(UserConnectionListener::CheckUserIP(), this);
			}
			else
			{
				fire(UserConnectionListener::FileNotAvailable(), this);
			}
		}
		/*#ifdef IRAINMAN_ENABLE_AUTO_BAN
		        else if (param.compare(0, 4, "BAN ", 4) == 0)   // !SMT!-B
		        {
		            fire(UserConnectionListener::BanMessage(), this, param); // !SMT!-B
		        }
		#endif*/
		else
		{
			dcdebug("Unknown $Error %s\n", param.c_str());
			fire(UserConnectionListener::ProtocolError(), this, param);
		}
	}
	else if (cmd == "Get")
	{
		x = param.find('$');
		if (x != string::npos)
		{
			fire(UserConnectionListener::Get(), this, Text::toUtf8(param.substr(0, x), m_last_encoding), Util::toInt64(param.substr(x + 1)) - (int64_t)1);
		}
	}
	else if (cmd == "Key")
	{
		if (!param.empty())
			fire(UserConnectionListener::Key(), this, param);
	}
	else if (cmd == "Lock")
	{
		if (!param.empty())
		{
			x = param.find(' ');
			fire(UserConnectionListener::CLock(), this, (x != string::npos) ? param.substr(0, x) : param);
		}
	}
	else if (cmd == "Send")
	{
		fire(UserConnectionListener::Send(), this);
	}
	else if (cmd == "MaxedOut")
	{
		fire(UserConnectionListener::MaxedOut(), this, param);
	}
	else if (cmd == "Supports")
	{
		if (!param.empty())
		{
			fire(UserConnectionListener::Supports(), this, StringTokenizer<string>(param, ' ').getTokensForWrite()); // [!] IRainman fix: http://code.google.com/p/flylinkdc/issues/detail?id=1112
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
			fire(UserConnectionListener::ListLength(), this, param);
		}
	}
	else
	{
		if (getUser() && aLine.length() < 255)
			ClientManager::getInstance()->setUnknownCommand(getUser(), aLine);
			
		dcdebug("Unknown NMDC command: %.50s\n", aLine.c_str());
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
		c.addParam(*i);
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
			fire(UserConnectionListener::ProtocolError(), this, c.getParam(1));
			return;
		}
	}
	
	fire(t, this, c);
}

void UserConnection::on(Connected) noexcept
{
	setLastActivity();
	fire(UserConnectionListener::Connected(), this);
}
void UserConnection::on(Data, uint8_t* p_data, size_t p_len) noexcept
{
	setLastActivity();
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
	if (p_len && BOOLSETTING(ENABLE_RATIO_USER_LIST))
		getUser()->AddRatioDownload(getSocket()->getIp4(), p_len);
#endif
	fire(UserConnectionListener::Data(), this, p_data, p_len);
}

void UserConnection::on(BytesSent, size_t p_Bytes, size_t p_Actual) noexcept
{
	setLastActivity();
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
	if (p_Actual && BOOLSETTING(ENABLE_RATIO_USER_LIST))
		getUser()->AddRatioUpload(getSocket()->getIp4(), p_Actual);
#endif
	fire(UserConnectionListener::BytesSent(), this, p_Bytes, p_Actual);
}

void UserConnection::on(ModeChange) noexcept
{
	setLastActivity();
	fire(UserConnectionListener::ModeChange(), this);
}

void UserConnection::on(TransmitDone) noexcept
{
	fire(UserConnectionListener::TransmitDone(), this);
}

void UserConnection::on(Updated) noexcept
{
	fire(UserConnectionListener::Updated(), this);
}

void UserConnection::on(Failed, const string& aLine) noexcept
{
	setState(STATE_UNCONNECTED);
	if (getUser()) // fix crash https://www.crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=44660
	{
		getUser()->fixLastIP();
	}
	fire(UserConnectionListener::Failed(), this, aLine);
	delete this;
}

// # ms we should aim for per segment
static const int64_t SEGMENT_TIME = 120 * 1000;
static const int64_t MIN_CHUNK_SIZE = 64 * 1024;

void UserConnection::updateChunkSize(int64_t leafSize, int64_t lastChunk, uint64_t ticks)
{

	if (chunkSize == 0)
	{
		chunkSize = std::max((int64_t)64 * 1024, std::min(lastChunk, (int64_t)1024 * 1024));
		return;
	}
	
	if (ticks <= 10)
	{
		// Can't rely on such fast transfers - double
		chunkSize *= 2;
		return;
	}
	
	double lastSpeed = (1000. * lastChunk) / ticks;
	
	int64_t targetSize = chunkSize;
	
	// How long current chunk size would take with the last speed...
	double msecs = 1000 * targetSize / lastSpeed;
	
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
		targetSize = std::max(MIN_CHUNK_SIZE, targetSize - chunkSize);
	}
	else
	{
		targetSize = std::max(MIN_CHUNK_SIZE, targetSize / 2);
	}
	
	chunkSize = targetSize;
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
		if (FavoriteManager::getInstance()->getFavUserParam(aUser, l_flags, limit))
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

/**
 * @file
 * $Id: UserConnection.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
