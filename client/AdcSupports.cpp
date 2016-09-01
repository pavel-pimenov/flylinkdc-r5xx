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
#include "AdcSupports.h"
#include "OnlineUser.h"

string AdcSupports::getSupports(const Identity& id)
{
	string tmp;
	const auto& u = id.getUser();
	
#define CHECK_FEAT(feat) if (u->isSet(User::feat)) { tmp += feat##_FEATURE + ' '; }
	
#define CHECK_SUP(sup) if (u->isSet(User::sup)) { tmp += sup##_SUPPORT + ' '; }
	
	CHECK_FEAT(TCP4);
	CHECK_FEAT(UDP4);
	CHECK_FEAT(TCP6);
	CHECK_FEAT(UDP6);
	CHECK_FEAT(ADCS);
	CHECK_FEAT(NAT0);
#ifdef STRONG_USE_DHT
	CHECK_SUP(DHT0);
#endif
	
#undef CHECK_FEAT
	
#undef CHECK_SUP
	
	const auto su = id.getKnownSupports();
	
#define CHECK_FEAT(feat) if (su & feat##_FEATURE_BIT) { tmp += feat##_FEATURE + ' '; }
	
	CHECK_FEAT(SEGA);
	
#undef CHECK_FEAT
	
	return tmp;
}

void AdcSupports::setSupports(Identity& id, const StringList & su)
{
	uint8_t knownSupports = 0;
	auto& u = id.getUser();
	for (auto i = su.cbegin(); i != su.cend(); ++i)
	{
	
#define CHECK_FEAT(feat) if (*i == feat##_FEATURE) { u->setFlag(User::feat); }
	
#define CHECK_SUP(feat) if (*i == feat##_SUPPORT) { u->setFlag(User::feat); }
	
#define CHECK_SUP_BIT(feat) if (*i == feat##_FEATURE) { knownSupports |= feat##_FEATURE_BIT; }
	
		CHECK_FEAT(TCP4)
		else CHECK_FEAT(UDP4)
			else CHECK_FEAT(TCP6)
				else CHECK_FEAT(UDP6)
					else CHECK_FEAT(ADCS)
						else CHECK_FEAT(NAT0)
#ifdef STRONG_USE_DHT
							else CHECK_SUP(DHT0)
#endif
								else CHECK_SUP_BIT(SEGA)
								
#ifdef FLYLINKDC_COLLECT_UNKNOWN_FEATURES
									else
									{
										//dcassert(0);
										CFlyFastLock(g_debugCsUnknownAdcFeatures);
										g_debugUnknownAdcFeatures[*i] = Util::toString(su);
									}
#endif
									
#undef CHECK_FEAT
									
#undef CHECK_SUP
									
#undef CHECK_SUP_BIT
									
	}
	id.setKnownSupports(knownSupports);
}

void AdcSupports::setSupports(Identity& id, const string & su)
{
	setSupports(id, StringTokenizer<string>(su, ',').getTokensForWrite());
}


string NmdcSupports::getStatus(const Identity& id) // [<-] moved from Util and review.
{
	const auto& u = id.getUser();
	string status = STRING(NORMAL) + ' ';
	uint8_t code = NORMAL;
	
	static const string g_tls = "TLS";
	static const string g_nat = "NAT-T";
#define CHECK_ST(flag, str) if (u->isSet(User::flag)) { status += str + ' '; code |= flag; }
	
	CHECK_ST(AWAY, STRING(AWAY));
	CHECK_ST(SERVER, STRING(FILESERVER));
	CHECK_ST(FIREBALL, STRING(FIREBALL));
	CHECK_ST(TLS, g_tls);
	CHECK_ST(NAT0, g_nat);
	/*
	CHECK_ST(X1, "X1");
	CHECK_ST(X1, "X2");
	*/
	
#undef CHECK_ST
	
	return status + '(' + Util::toString(code) + ')';
}

void NmdcSupports::setStatus(Identity& id, const char status, const string& connection /* = Util::emptyString */)
{
	if (!connection.empty())
	{
		auto con = Util::toDouble(connection);
		double coef;
		const auto i = connection.find_first_not_of("1234567890 ,."); // TODO - отложить парсинг соединения позже
		if (i == string::npos)
		{
			coef = 1000 * 1000 / 8; // no postfix - megabits.
		}
		else
		{
			auto postfix = connection.substr(i);
			
#define CHECK_SPEED(s, c) if (postfix == s) { coef = c; }
			
			CHECK_SPEED("KiB/s", 1024)
			/*
			else CHECK_SPEED("MiB/s", 1024 * 1024)
			else CHECK_SPEED("B/s", 1)
			*/
			// http://nmdc.sourceforge.net/NMDC.html#_myinfo
			// Connection is a string for the connection:
			// Default NMDC1 connections types 28.8Kbps, 33.6Kbps, 56Kbps, Satellite, ISDN, DSL, Cable, LAN(T1), LAN(T3)
			// Default NMDC2 connections types Modem, DSL, Cable, Satellite, LAN(T1), LAN(T3)
			else CHECK_SPEED("Kbps", 1000)
				else
				{
					coef = 0;
#ifdef FLYLINKDC_COLLECT_UNKNOWN_FEATURES
					// CFlyFastLock(g_debugCsUnknownNmdcConnection);
					// g_debugUnknownNmdcConnection.insert(postfix);
#endif
				}
#undef CHECK_SPEED
		}
		con *= coef;
		id.setDownloadSpeed(static_cast<uint32_t>(con));
	}
	
	const auto& u = id.getUser();
	
#define CHECK_ST(flag) if ((status & flag) != 0) { u->setFlag(User::flag); }
	
	CHECK_ST(AWAY);
	CHECK_ST(SERVER);
	CHECK_ST(FIREBALL);
	CHECK_ST(TLS);
	CHECK_ST(NAT0);
	/* TODO
	CHECK_ST(X1);
	CHECK_ST(X2);
	*/
#undef CHECK_ST
	
}
/*
string NmdcSupports::getSupports(const Identity& id)
{
    string tmp;

    return tmp;
}

void NmdcSupports::setSupports(Identity& id, StringList & su)
{
    //uint8_t knownSupports = 0;
    for (auto i = su.begin(); i != su.end(); ++i)
    {
        //if ()
        {
            //id.setKnownSupports();
        }
#ifdef _DEBUG
        //else
        {
            CFlyFastLock(AdcSupports::_debugCsUnknownFeatures);
            AdcSupports::_debugUnknownFeatures.insert(*i);
        }
#endif
    }
    //id.setKnownSupports(knownSupports);
}
*/
