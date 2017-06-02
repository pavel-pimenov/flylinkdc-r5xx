/*
 *
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

#ifndef USERINFO_COLUMNS_H
#define USERINFO_COLUMNS_H


enum
{
	COLUMN_FIRST,
	COLUMN_NICK = COLUMN_FIRST,
	COLUMN_SHARED,
	COLUMN_EXACT_SHARED,
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
	COLUMN_ANTIVIRUS,
#endif
	COLUMN_DESCRIPTION,
	COLUMN_APPLICATION,
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
	COLUMN_CONNECTION,
#endif
	COLUMN_EMAIL,
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
	COLUMN_VERSION,
	COLUMN_MODE,
#endif
	COLUMN_HUBS,
	COLUMN_SLOTS,
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
	COLUMN_UPLOAD_SPEED,
#endif
	COLUMN_IP,
	COLUMN_GEO_LOCATION,
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	COLUMN_UPLOAD,
	COLUMN_DOWNLOAD,
	COLUMN_MESSAGES,
#endif
#ifdef FLYLINKDC_USE_DNS
	COLUMN_DNS, // !SMT!-IP
#endif
//[-]PPA        COLUMN_PK,
	COLUMN_CID,
	COLUMN_TAG,
	COLUMN_P2P_GUARD,
#ifdef FLYLINKDC_USE_EXT_JSON
	COLUMN_FLY_HUB_GENDER,
#ifdef FLYLINKDC_USE_LOCATION_DIALOG
	COLUMN_FLY_HUB_COUNTRY,
	COLUMN_FLY_HUB_CITY,
	COLUMN_FLY_HUB_ISP,
#endif
	COLUMN_FLY_HUB_COUNT_FILES,
	COLUMN_FLY_HUB_LAST_SHARE_DATE,
	COLUMN_FLY_HUB_RAM,
	COLUMN_FLY_HUB_SQLITE_DB_SIZE,
	COLUMN_FLY_HUB_QUEUE,
	COLUMN_FLY_HUB_TIMES,
	COLUMN_FLY_HUB_SUPPORT_INFO,
#endif
	COLUMN_LAST
};


#endif // USERINFO_COLUMNS_H
