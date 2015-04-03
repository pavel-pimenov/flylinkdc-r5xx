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

#ifndef DCPLUSPLUS_DCPP_FORWARD_H_
#define DCPLUSPLUS_DCPP_FORWARD_H_

/** @file
 * This file contains forward declarations for the various DC++ classes
 */

#include <boost/intrusive_ptr.hpp>

class AdcCommand;

class ADLSearch;

class BufferedSocket;

class CID;

typedef std::vector<uint16_t> PartsInfo;

class Client;

class ClientManager;

class ConnectionQueueItem;

class DirectoryItem;
typedef DirectoryItem* DirectoryItemPtr;

class FavoriteHubEntry;
typedef FavoriteHubEntry* FavoriteHubEntryPtr;
typedef std::vector<FavoriteHubEntryPtr> FavoriteHubEntryList;

class FavoriteUser;

class File;

class FinishedItem;
typedef std::deque<FinishedItem*> FinishedItemList; // [!] IRainman opt: change vector to deque

class FinishedManager;

template<class Hasher>
struct HashValue;

//struct HintedUser;
// [-] typedef std::vector<HintedUser> HintedUserList; [-] IRainman: deprecated.

class HubEntry;
typedef std::deque<HubEntry> HubEntryList; // [!] IRainman opt: change vector to deque

class Identity;

class InputStream;

class LogManager;

class OnlineUser;
typedef boost::intrusive_ptr<OnlineUser> OnlineUserPtr;
typedef std::vector<OnlineUserPtr> OnlineUserList;

class OutputStream;

class QueueItem;
typedef boost::intrusive_ptr<QueueItem> QueueItemPtr; // [!] IRainman fix.
typedef std::list<QueueItemPtr> QueueItemList;

class RecentHubEntry;

class ServerSocket;

class Socket;
class SocketException;

class StringSearch;

class TigerHash;

//class Transfer;

typedef HashValue<TigerHash> TTHValue;

//class UnZFilter;

class Upload;

class UploadQueueItem;
typedef UploadQueueItem* UploadQueueItemPtr;

// http://code.google.com/p/flylinkdc/issues/detail?id=1413
//class UploadQueueItemInfo;
//typedef UploadQueueItemInfo* UploadQueueItemInfoPtr;

class User;
typedef boost::intrusive_ptr<User> UserPtr;
typedef std::vector<UserPtr> UserList;

class UserCommand;
class UserConnection;

#endif /*DCPLUSPLUS_CLIENT_FORWARD_H_*/
