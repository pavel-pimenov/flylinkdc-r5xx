#ifndef CLIENTLISTENER_H_
#define CLIENTLISTENER_H_

#include "SearchQueue.h"

#include "ChatMessage.h"

class ClientListener
{
	public:
		virtual ~ClientListener() { }
		template<int I> struct X
		{
			enum { TYPE = I };
		};
		
		typedef X<0> Connecting;
		typedef X<1> Connected;
		typedef X<3> UserUpdated;
		typedef X<4> UsersUpdated;
		typedef X<5> UserRemoved;
		typedef X<6> Redirect;
		typedef X<7> Failed;
		typedef X<8> GetPassword;
		typedef X<9> HubUpdated;
		typedef X<11> Message;
		typedef X<12> StatusMessage;
		typedef X<13> HubUserCommand;
		typedef X<14> HubFull;
		typedef X<15> NickTaken;
		typedef X<17> NmdcSearch;
		typedef X<18> AdcSearch;
		typedef X<19> CheatMessage;
		typedef X<20> HubTopic;
		typedef X<21> UserReport;
		// typedef X<22> PrivateMessage;// !SMT!-S [-] IRainman fix.
		
		enum StatusFlags
		{
			FLAG_NORMAL = 0x00,
			FLAG_IS_SPAM = 0x01
		};
		
		virtual void on(Connecting, const Client*) noexcept { }
		virtual void on(Connected, const Client*) noexcept { }
		virtual void on(UserUpdated, const OnlineUserPtr&) noexcept { }
		virtual void on(UsersUpdated, const Client*, const OnlineUserList&) noexcept { }
		virtual void on(UserRemoved, const Client*, const OnlineUserPtr&) noexcept { }
		virtual void on(Redirect, const Client*, const string&) noexcept { }
		virtual void on(Failed, const Client*, const string&) noexcept { }
		virtual void on(GetPassword, const Client*) noexcept { }
		virtual void on(HubUpdated, const Client*) noexcept { }
		virtual void on(Message, const Client*, std::unique_ptr<ChatMessage>&) noexcept { }  // !SMT!-S
		virtual void on(StatusMessage, const Client*, const string&, int = FLAG_NORMAL) noexcept { }
		virtual void on(HubUserCommand, const Client*, int, int, const string&, const string&) noexcept { }
		virtual void on(HubFull, const Client*) noexcept { }
		virtual void on(NickTaken, const Client*) noexcept { }
		virtual void on(NmdcSearch, Client* aClient, const string& aSeeker, Search::SizeModes aSizeMode, int64_t aSize,
		                Search::TypeModes aFileType, const string& aString, bool isPassive) noexcept { }
		virtual void on(AdcSearch, const Client*, const AdcCommand&, const CID&) noexcept { }
		virtual void on(CheatMessage, const string&) noexcept { }
		virtual void on(HubTopic, const Client*, const string&) noexcept { }
		//virtual void on(UserReport, const Client*, const Identity&) noexcept { }
		virtual void on(UserReport, const Client*, const string&) noexcept { } // [!] IRainman fix
		//virtual void on(PrivateMessage, const Client*, const string &strFromUserName, const OnlineUserPtr&, const OnlineUserPtr&, const OnlineUserPtr&, const string&, bool = true) noexcept { } // !SMT!-S  [-] IRainman fix.
};

#endif /*CLIENTLISTENER_H_*/