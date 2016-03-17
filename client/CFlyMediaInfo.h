//-----------------------------------------------------------------------------
//(c) 2011-2016 pavel.pimenov@gmail.com
//-----------------------------------------------------------------------------
#ifndef CFlyMediaInfo_H
#define CFlyMediaInfo_H

#pragma once

#include <string>
#include <vector>
#include "Text.h"
#include "SimpleXML.h"

#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
#include <boost/functional/hash.hpp>

struct CFlyServerCache
{
	std::string m_ratio;
	std::string m_antivirus;
	std::string m_video;
	std::string m_audio;
	std::string m_audio_br;
	std::string m_xy;
	CFlyServerCache()
	{
	}
};
#endif

#ifdef _DEBUG
class CFlyMediainfoRAW
{
	public:
		string m_WH;
		string m_br;
		string m_audio;
		string m_video;
		bool operator==(const CFlyMediainfoRAW& p_val) const
		{
			return m_audio == p_val.m_audio
			       && m_br == p_val.m_br
			       && m_WH == p_val.m_WH
			       && m_video == p_val.m_video;
		}
};
struct CFlyMediainfoRAWHasher
{
	std::size_t operator()(const CFlyMediainfoRAW& p_val) const
	{
		using boost::hash_value;
		using boost::hash_combine;
		std::size_t seed = 0;
		hash_combine(seed, hash_value(p_val.m_audio));
		hash_combine(seed, hash_value(p_val.m_br));
		hash_combine(seed, hash_value(p_val.m_WH));
		hash_combine(seed, hash_value(p_val.m_video));
		return seed;
	}
};

#endif

class CFlyMediaInfo
#ifdef _DEBUG
	//: public boost::noncopyable
#endif
{
	public:
		struct ExtItem
		{
			enum
			{
				channel_all = 255
			};
			bool   m_is_delete;
			uint8_t m_stream_type; // MediaInfoLib::stream_t
			uint8_t m_channel; // если = channel_all, то это общий канал дл€ всего типа (пока используетс€ дл€ оптимизации Audio)
			string m_param;
			string m_value;
			ExtItem() : m_stream_type(0), m_channel(0), m_is_delete(false)
			{
			}
		};
		std::vector<ExtItem> m_ext_array; // TODO: add interface
		
		uint16_t m_bitrate;
		uint16_t m_mediaX;
		uint16_t m_mediaY;
		std::string m_video;
		std::string m_audio;
		bool m_is_need_escape;
		CFlyMediaInfo()
		{
			init();
		}
		CFlyMediaInfo(const std::string& p_WH, uint16_t p_bitrate,
		              const std::string& p_audio, const std::string& p_video): m_audio(p_audio), m_video(p_video)
		{
			dcassert(!m_audio.empty() || !m_video.empty());
			init(p_WH, p_bitrate);
		}
		CFlyMediaInfo(const std::string& p_WH, uint16_t p_bitrate = 0)
		{
			init(p_WH, p_bitrate);
		}
#ifdef _DEBUG
		~CFlyMediaInfo()
		{
			int a = 0;
			a++;
		}
#endif
		void calcEscape()
		{
			m_is_need_escape = SimpleXML::needsEscapeForce(m_audio) || SimpleXML::needsEscapeForce(m_video);
		}
		bool isMedia() const
		{
			return m_bitrate != 0 || !m_audio.empty() || !m_video.empty();
		}
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
		bool isMediaAttrExists() const
		{
			return !m_ext_array.empty();
		}
#endif
		void init(const std::string& p_WH, uint16_t p_bitrate = 0)
		{
			init(p_bitrate);
			const auto l_pos = p_WH.find('x');
			if (l_pos != std::string::npos)
			{
				m_mediaX = atoi(p_WH.c_str());
				m_mediaY = atoi(p_WH.c_str() + l_pos + 1);
			}
		}
		void init(uint16_t p_bitrate = 0)
		{
			m_bitrate = p_bitrate;
			m_mediaX  = 0;
			m_mediaY  = 0;
			m_is_need_escape = false;
		}
		
		string getXY() const
		{
			if (m_mediaX && m_mediaY)
			{
				char l_buf[30];
				l_buf[0] = 0;
				_snprintf(l_buf, _countof(l_buf), "%ux%u", m_mediaX, m_mediaY);
				return l_buf;
			}
			return string();
		}
		static void translateDuration(const string& p_audio,
		                              tstring& p_column_audio,
		                              tstring& p_column_duration)  // ќтрезалка длительности из пол€ Audio
		{
			p_column_duration.clear();
			p_column_audio.clear();
			if (!p_audio.empty())
			{
				const size_t l_pos = p_audio.find('|', 0);
				if (l_pos != string::npos && l_pos)
				{
					if (p_audio.size() > 6 && p_audio[0] >= '1' && p_audio[0] <= '9' && // ѕроверим факт наличи€ в начале длительности
					        (p_audio[1] == 'm' || p_audio[2] == 'n') || // "1mn XXs"
					        (p_audio[1] == 's' || p_audio[2] == ' ') || // "1s XXXms"
					        (p_audio[2] == 's' || p_audio[3] == ' ') || // "59s XXXms"
					        (p_audio[2] == 'm' || p_audio[3] == 'n') ||   // "59mn XXs"
					        (p_audio[1] == 'h') ||  // "1h XXmn"
					        (p_audio[2] == 'h')     // "60h XXmn"
					   )
					{
						Text::toT(p_audio.substr(0, l_pos - 1), p_column_duration);
						if (l_pos + 2 < p_audio.length())
							Text::toT(p_audio.substr(l_pos + 2), p_column_audio);
					}
					else
					{
						p_column_audio = Text::toT(p_audio); // ≈сли не распарсили - показывает что есть
						//dcassert(0); // fix "1 076 Kbps,23mn 9s | MPEG Audio, 160 Kbps, 2 channels"
					}
				}
			}
		}
};

#ifdef _DEBUG
typedef std::unordered_map<CFlyMediainfoRAW, std::shared_ptr<CFlyMediaInfo>, CFlyMediainfoRAWHasher> CFlyCacheMediaInfo;
#endif

struct CFlyHashCacheItem
{
	__int64 m_path_id;
	int64_t m_size;
	int64_t m_time_stamp;
	TigerTree m_tth;
	CFlyMediaInfo m_out_media;
	CFlyHashCacheItem(__int64 p_path_id, int64_t p_time_stamp, const TigerTree& p_tth, int64_t p_size, CFlyMediaInfo& p_out_media)
		: m_path_id(p_path_id), m_time_stamp(p_time_stamp), m_tth(p_tth), m_size(p_size), m_out_media(p_out_media)
	{
	}
};


#endif
