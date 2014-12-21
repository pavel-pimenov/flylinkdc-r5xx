#ifndef UPLOAD_H_
#define UPLOAD_H_

#include "Transfer.h"
#include "Flags.h"

class Upload : public Transfer, public Flags
#ifdef _DEBUG
	, virtual NonDerivable<Upload> // [+] IRainman fix.
#endif
{
	public:
		enum Flags
		{
			FLAG_ZUPLOAD = 0x01,
			FLAG_PENDING_KICK = 0x02,
			FLAG_RESUMED = 0x04, //-V112
			FLAG_CHUNKED = 0x08,
			FLAG_PARTIAL = 0x10
		};
		
		explicit Upload(UserConnection* p_conn, const TTHValue& p_tth, const string& p_path, const string& p_ip, const string& p_chiper_name); // [!] IRainman fix.
		~Upload();
		
		void getParams(const UserConnection* p_source, StringMap& p_params) const;
		
	private:
#ifdef IRAINMAN_USE_NG_TRANSFERS
		const string path; // TODO: maybe it makes sense to back up the line after the original problem will be eliminated. In the Upload to inheritance so not very nice.
#endif
		GETSET(int64_t, fileSize, FileSize);
		GETSET(InputStream*, m_stream, Stream);
		
		uint8_t m_delayTime;
};

#endif /*UPLOAD_H_*/
