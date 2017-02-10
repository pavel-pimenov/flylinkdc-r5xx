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
#pragma once

#ifndef DCPLUSPLUS_DCPP_ADC_SUPPORTS_H
#define DCPLUSPLUS_DCPP_ADC_SUPPORTS_H

#include "AdcCommand.h"
class Identity;
class AdcSupports // [+] IRainman fix.
{
	public:
		static const string CLIENT_PROTOCOL;
		static const string SECURE_CLIENT_PROTOCOL_TEST;
		static const string ADCS_FEATURE;
		static const string TCP4_FEATURE;
		static const string TCP6_FEATURE;
		static const string UDP4_FEATURE;
		static const string UDP6_FEATURE;
		static const string NAT0_FEATURE;
		static const string SEGA_FEATURE;
		static const string BASE_SUPPORT;
		static const string BAS0_SUPPORT;
		static const string TIGR_SUPPORT;
		static const string UCM0_SUPPORT;
		static const string BLO0_SUPPORT;
#ifdef STRONG_USE_DHT
		static const string DHT0_SUPPORT;
#endif
		static const string ZLIF_SUPPORT;
		
		enum KnownSupports
		{
			SEGA_FEATURE_BIT = 1,
		};
		
		static string getSupports(const Identity& id);
		static void setSupports(Identity& id, const StringList & su);
		static void setSupports(Identity& id, const string & su);
		
#ifdef FLYLINKDC_COLLECT_UNKNOWN_FEATURES
		static FastCriticalSection g_debugCsUnknownAdcFeatures;
		static boost::unordered_map<string, string> g_debugUnknownAdcFeatures;
#endif
};

class NmdcSupports // [+] IRainman fix.
{
	public:
		enum Status // [<-] from Identity.
		{
			NORMAL      = 0x01,
			AWAY        = 0x02,
			SERVER      = 0x04, //-V112
			FIREBALL    = 0x08,
			TLS         = 0x10,
			NAT0        = 0x20, // TODO
			//X1        = 0x40, // TODO
			//X2        = 0x80, // TODO
		};
		static string getStatus(const Identity& id); // [<-] moved from Util and review.
		static void setStatus(Identity& id, const char status, const string& connection = Util::emptyString);
		static string getSupports(const Identity& id);
		static void setSupports(Identity& id, const StringList & su);
#ifdef FLYLINKDC_COLLECT_UNKNOWN_FEATURES
		static FastCriticalSection g_debugCsUnknownNmdcConnection;
		static boost::unordered_set<string> g_debugUnknownNmdcConnection;
#endif // FLYLINKDC_COLLECT_UNKNOWN_FEATURES
#ifdef FLYLINKDC_COLLECT_UNKNOWN_TAG
		static FastCriticalSection g_debugCsUnknownNmdcTagParam;
		static boost::unordered_map<string, unsigned> g_debugUnknownNmdcTagParam;
#endif // FLYLINKDC_COLLECT_UNKNOWN_TAG
};

#endif // DCPLUSPLUS_DCPP_ADC_SUPPORTS_H