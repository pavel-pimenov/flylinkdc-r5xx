
#pragma once

#ifndef UPLOAD_H_
#define UPLOAD_H_

#include "Transfer.h"
#include "Flags.h"
class InputStream;

class Upload : public Transfer, public Flags
{
	public:
		enum Flags
		{
			FLAG_ZUPLOAD = 0x01,
			FLAG_PENDING_KICK = 0x02,
			FLAG_RESUMED = 0x04, //-V112
			FLAG_CHUNKED = 0x08,
			FLAG_UPLOAD_PARTIAL = 0x10
		};
		
		explicit Upload(UserConnection* p_conn, const TTHValue& p_tth, const string& p_path, const string& p_ip, const string& p_chiper_name); // [!] IRainman fix.
		~Upload();
		
		void getParams(StringMap& p_params) const;
		
	private:
	
		GETSET(InputStream*, m_read_stream, ReadStream);
		
		uint8_t m_delayTime;
};

typedef std::shared_ptr<Upload> UploadPtr;

#endif /*UPLOAD_H_*/
