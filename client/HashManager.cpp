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
#include "HashManager.h"
#include "ResourceManager.h"
#include "SimpleXML.h"
#include "LogManager.h"
#include "CFlylinkDBManager.h"
#include "MediaInfo/MediaInfo.h"
#include "MediaInfo/MediaInfo_Config.h"
#include "ZenLib/ZtringListList.h"
#include <boost/algorithm/string.hpp>
#include "CompatibilityManager.h" // [+] IRainman
#include "../FlyFeatures/flyServer.h"

#ifdef IRAINMAN_NTFS_STREAM_TTH
//[+] Greylink
const string HashManager::StreamStore::g_streamName(".gltth");

#ifdef RIP_USE_STREAM_SUPPORT_DETECTION
void HashManager::StreamStore::SetFsDetectorNotifyWnd(HWND hWnd)
{
	m_FsDetect.SetNotifyWnd(hWnd);
}
#else
std::unordered_set<char> HashManager::StreamStore::g_error_tth_stream;
#endif

inline void HashManager::StreamStore::setCheckSum(TTHStreamHeader& p_header)
{
	p_header.magic = g_MAGIC;
	uint32_t l_sum = 0;
	for (size_t i = 0; i < sizeof(TTHStreamHeader) / sizeof(uint32_t); i++)
		l_sum ^= ((uint32_t*) & p_header)[i];
	p_header.checksum ^= l_sum;
}

inline bool HashManager::StreamStore::validateCheckSum(const TTHStreamHeader& p_header)
{
	if (p_header.magic != g_MAGIC)
		return false;
	uint32_t l_sum = 0;
	for (size_t i = 0; i < sizeof(TTHStreamHeader) / sizeof(uint32_t); i++)
		l_sum ^= ((uint32_t*) & p_header)[i];
	return (l_sum == 0);
}
void HashManager::addTree(const TigerTree& p_tree)
{
	CFlylinkDBManager::getInstance()->addTree(p_tree);
}

bool HashManager::StreamStore::loadTree(const string& p_filePath, TigerTree& p_Tree, int64_t p_FileSize)
{
	try
	{
#ifdef RIP_USE_STREAM_SUPPORT_DETECTION
		if (!m_FsDetect.IsStreamSupported(p_filePath.c_str()))
#else
		if (isBan(p_filePath))
#endif
			return false;
		dcassert(p_FileSize > 0);
		const int64_t l_fileSize = p_FileSize == -1 ? File::getSize(p_filePath) : p_FileSize;
		if (l_fileSize < SETTING(SET_MIN_LENGHT_TTH_IN_NTFS_FILESTREAM) * 1048576) // that's why minStreamedFileSize never be changed![*]NightOrion
			return false;
		const int64_t l_timeStamp = File::getTimeStamp(p_filePath);
		{
			File l_stream(p_filePath + ":" + g_streamName, File::READ, File::OPEN);
			size_t l_sz = sizeof(TTHStreamHeader);
			TTHStreamHeader h;
			if (l_stream.read(&h, l_sz) != sizeof(TTHStreamHeader))
				return false;
			if (uint64_t(l_fileSize) != h.fileSize || l_timeStamp != uint64_t(h.timeStamp) || !validateCheckSum(h))
				return false;
			const size_t l_datalen = TigerTree::calcBlocks(l_fileSize, h.blockSize) * TTHValue::BYTES;
			l_sz = l_datalen;
			AutoArray<uint8_t> buf(l_datalen);
			if (l_stream.read((uint8_t*)buf, l_sz) != l_datalen)
				return false;
			p_Tree = TigerTree(l_fileSize, h.blockSize, buf, l_datalen);
			if (!(p_Tree.getRoot() == h.root))
				return false;
		}
	}
	catch (const Exception& /*e*/)
	{
		// Отключил спам http://code.google.com/p/flylinkdc/issues/detail?id=1415
		// LogManager::getInstance()->message(STRING(ERROR_GET_TTH_STREAM) + ' ' + p_filePath + " Error = " + e.getError());// [+]IRainman
		return false;
	}
	/*
	if (speed > 0) {
	        LogManager::getInstance()->message(STRING(HASHING_FINISHED) + ' ' + fn + " (" + Util::formatBytes(speed) + "/s)");
	    } else {
	        LogManager::getInstance()->message(STRING(HASHING_FINISHED) + ' ' + fn);
	    }
	*/
	return true;
}

bool HashManager::StreamStore::saveTree(const string& p_filePath, const TigerTree& p_Tree)
{
	if (CompatibilityManager::isWine() || //[+]PPA под wine не пишем в поток
	        !BOOLSETTING(SAVE_TTH_IN_NTFS_FILESTREAM))
		return false;
#ifdef RIP_USE_STREAM_SUPPORT_DETECTION
	if (!m_FsDetect.IsStreamSupported(p_filePath.c_str()))
#else
	if (isBan(p_filePath))
#endif
		return false;
	try
	{
		TTHStreamHeader h;
		h.fileSize = File::getSize(p_filePath);
		if (h.fileSize < (SETTING(SET_MIN_LENGHT_TTH_IN_NTFS_FILESTREAM) * 1048576) || h.fileSize != (uint64_t)p_Tree.getFileSize()) //[*]NightOrion
			return false; // that's why minStreamedFileSize never be changed!
		h.timeStamp = File::getTimeStamp(p_filePath);
		h.root = p_Tree.getRoot();
		h.blockSize = p_Tree.getBlockSize();
		setCheckSum(h);
		{
			File stream(p_filePath + ":" + g_streamName, File::WRITE, File::CREATE | File::TRUNCATE);
			stream.write(&h, sizeof(TTHStreamHeader));
			stream.write(p_Tree.getLeaves()[0].data, p_Tree.getLeaves().size() * TTHValue::BYTES);
		}
		File::setTimeStamp(p_filePath, h.timeStamp);
	}
	catch (const Exception& e)
	{
#ifndef RIP_USE_STREAM_SUPPORT_DETECTION
		addBan(p_filePath);
#endif
		LogManager::getInstance()->message(STRING(ERROR_ADD_TTH_STREAM) + ' ' + p_filePath + " : " + e.getError());// [+]IRainman
		return false;
	}
	return true;
}
//[~] Greylink

void HashManager::StreamStore::deleteStream(const string& p_filePath)
{
	try
	{
		File::deleteFile(p_filePath + ":" + g_streamName);
	}
	catch (const FileException& e)
	{
		LogManager::getInstance()->message(STRING(ERROR_DELETE_TTH_STREAM) + ' ' + p_filePath + " : " + e.getError());
	}
}

#ifdef RIP_USE_STREAM_SUPPORT_DETECTION
void HashManager::SetFsDetectorNotifyWnd(HWND hWnd)
{
	m_streamstore.SetFsDetectorNotifyWnd(hWnd);
}
#endif

bool HashManager::addFileFromStream(const string& p_name, const TigerTree& p_TT, int64_t p_size)
{
	const int64_t l_TimeStamp = File::getTimeStamp(p_name);
	CFlyMediaInfo l_out_media;
	return addFile(p_name, l_TimeStamp, p_TT, p_size, l_out_media);
}
#endif // IRAINMAN_NTFS_STREAM_TTH

string g_cur_mediainfo_file;
string g_cur_mediainfo_file_tth;
//=========================================================================================
static void getExtMediaInfo(const string& p_file_ext_wo_dot,
                            int64_t p_size,
                            MediaInfoLib::MediaInfo& p_media_info_dll,
                            MediaInfoLib::stream_t p_stream_type,
                            CFlyMediaInfo& p_media,
                            bool p_compress_channel_attr)
{
	const ZtringListList l_info = MediaInfoLib::Config.Info_Get(p_stream_type);
	if (const size_t l_count = p_media_info_dll.Count_Get(p_stream_type))
	{
		int l_count_audio_channel = p_stream_type == MediaInfoLib::Stream_Audio ? l_count : 0; // Число каналов считаем только для Audio
		for (auto i = l_info.cbegin() ; i != l_info.cend(); ++i)
		{
			const auto l_param_name = i->Read(0);
			for (size_t j = 0; j < l_count; ++j)
			{
				const auto l_value = p_media_info_dll.Get(p_stream_type, j, l_param_name);
				if (!l_value.empty())
				{
					const string l_str_param_name = Text::fromT(l_param_name);
					if (g_fly_server_config.isSupportTag(l_str_param_name)) // TODO Fix fromT
					{
						CFlyMediaInfo::ExtItem l_ext_item;
						l_ext_item.m_stream_type = p_stream_type;
						l_ext_item.m_channel = j;
						l_ext_item.m_param = l_str_param_name;
						l_ext_item.m_value = Text::fromT(l_value);
						// Inform - Сводный параметр - распарсим его для удаления лишних записей и пробелов.
						if (l_str_param_name.compare(0, 6, "Inform", 6) == 0)
						{
							g_fly_server_config.ConvertInform(l_ext_item.m_value);
						}
						p_media.m_ext_array.push_back(l_ext_item);
					}
				}
			}
		}
		if (p_compress_channel_attr && l_count_audio_channel) // Если аудио-каналов несколько. дополнительно обработаем массив
		{
			// Упакуем массив каналов если их несколько и значения признаков одинаковое для всех.
			// Пока алгоритм обработки двух-проходный
			// избыточные атрибуты для каналов пометим флажком чтобы не писать в базу.
			// TODO - протестировать и физическое удаление лишних записей из вектора
			std::map <std::string, std::pair< std::string, int> > l_channel_dup_filter;
			// Параметр MI - пара - { значение параметра + счетчик различных вариантов для значений }
			for (auto j = p_media.m_ext_array.cbegin(); j != p_media.m_ext_array.cend(); ++j)
				// TODO [!] этот проход можно поместить внуть основного алгоритма заполнения.
				// ведь нам известно заранее что на Audio - несколько каналов будет
			{
				dcassert(j->m_stream_type == MediaInfoLib::Stream_Audio);
				if (j->m_stream_type == MediaInfoLib::Stream_Audio)
				{
					auto& l_value = l_channel_dup_filter[j->m_param];
					if (l_value.first != j->m_value)
					{
						if (l_value.first.empty()) // Первый параметр?
						{
							l_value.first  = j->m_value;
						}
						l_value.second++;
					}
				}
			}
			// Расчитали статистику по атрибутам.
			// Отметим дубликатные записи флажком для последующего игнора при записи в базу данных.
			for (auto k = p_media.m_ext_array.begin(); k != p_media.m_ext_array.end(); ++k)
			{
				dcassert(k->m_stream_type == MediaInfoLib::Stream_Audio);
				if (k->m_stream_type == MediaInfoLib::Stream_Audio)
				{
					auto l_channel_filter = l_channel_dup_filter.find(k->m_param);
					dcassert(l_channel_filter != l_channel_dup_filter.end());
					if (l_channel_filter->second.second == 1) // Для всех каналов значения параметра одинаковое?
					{
						k->m_channel  = CFlyMediaInfo::ExtItem::channel_all; // Канал для первого вхождения делаем 255 (channel-All)
						l_channel_filter->second.second = -1; // Скидываем счетчик в -1 чтобы на следуещем проходе удалить запись
						continue;
					}
					if (l_channel_filter->second.second == -1) // Значение параметра уже перенесено в общий канал с кодом 255 на предыдущей итеррации
						// а текущая запись избыточная и ее писать в базу не нужно.
						// TODO - удаление из vector-а пока не делаем. позже эта запись всеравно не загрузится
					{
						k->m_is_delete = true;
					}
				}
			}
		}
	}
}
//=========================================================================================
bool HashManager::getMediaInfo(const string& p_name, CFlyMediaInfo& p_media, int64_t p_size, const TTHValue& p_tth, bool p_force /* = false*/)
{
	try
	{
		static MediaInfoLib::MediaInfo g_media_info_dll;
		if (p_size < SETTING(MIN_MEDIAINFO_SIZE) * 1024 * 1024) // TODO: p_size?
			return false;
		const string l_file_ext = Text::toLower(Util::getFileExtWithoutDot(p_name));
		if (!g_fly_server_config.isMediainfoExt(l_file_ext))
			return false;
		char l_size[22];
		l_size[0] = 0;
		_snprintf(l_size, _countof(l_size), "%I64d", p_size);
		g_cur_mediainfo_file_tth = p_tth.toBase32();
		g_cur_mediainfo_file = p_name + "\r\n TTH = " + g_cur_mediainfo_file_tth + "\r\n File size = " + string(l_size);
		if (g_media_info_dll.Open(Text::toT(File::formatPath(p_name))))
		{
			// const bool l_is_media_info_fly_server = g_fly_server_config.isSupportFile(l_file_ext, p_size);
			// if (l_is_media_info_fly_server)
			// Локально собираем медиаинфу всегда - чтобы проще расширять поддерживаемые расширения на флай сервере в будущем.
			{
				// TODO - желательно звать со сжатием каналов первым (будет быстрее из-за подсказки p_compress_channel_attr)
				getExtMediaInfo(l_file_ext, p_size, g_media_info_dll, MediaInfoLib::Stream_Audio, p_media, true); // Сожмем дубликаты в каналах
				getExtMediaInfo(l_file_ext, p_size, g_media_info_dll, MediaInfoLib::Stream_General, p_media, false);
				getExtMediaInfo(l_file_ext, p_size, g_media_info_dll, MediaInfoLib::Stream_Video, p_media, false);
// Это пока лишнее
//          getExtMediaInfo(l_file_ext, p_size, g_media_info_dll,MediaInfoLib::Stream_Text,p_media);
//			getExtMediaInfo(l_file_ext, p_size, g_media_info_dll,MediaInfoLib::Stream_Chapters,p_media);
//			getExtMediaInfo(l_file_ext, p_size, g_media_info_dll,MediaInfoLib::Stream_Image,p_media);
//			getExtMediaInfo(l_file_ext, p_size, g_media_info_dll,MediaInfoLib::Stream_Menu,p_media);
			}
			const size_t audioCount = g_media_info_dll.Count_Get(MediaInfoLib::Stream_Audio);
			p_media.m_bitrate  = 0;
			boost::unordered_map<string, uint16_t> l_audio_dup_filter;
			// AC-3, 5.1, 448 Kbps | AC-3, 5.1, 640 Kbps | TrueHD / AC-3, 5.1, 640 Kbps | AC-3, 5.1, 448 Kbps | AC-3, 5.1, 448 Kbps | AC-3, 5.1, 448 Kbps | AC-3, 5.1, 448 Kbps | AC-3, 5.1, 448 Kbps | AC-3, 5.1, 448 Kbps"
			// Превращаем в
			// AC-3, 5.1, 640 Kbps | TrueHD / AC-3, 5.1, 640 Kbps | AC-3, 5.1, 448 Kbps (x7)
			dcassert(audioCount);
			for (size_t i = 0; i < audioCount; i++)
			{
				const wstring l_sinfo = g_media_info_dll.Get(MediaInfoLib::Stream_Audio, i, _T("BitRate"));
				uint16_t bitRate = (Util::toFloat(Text::fromT(l_sinfo)) / 1000.0 + 0.5);
				if (bitRate > p_media.m_bitrate)
					p_media.m_bitrate = bitRate;
				wstring sFormat = g_media_info_dll.Get(MediaInfoLib::Stream_Audio, i, _T("Format"));
#if defined (SSA_REMOVE_NEEDLESS_WORDS_FROM_VIDEO_AUDIO_INFO)
				boost::replace_all(sFormat, _T(" Audio"), Util::emptyStringT);
#endif
				const wstring sBitRate = g_media_info_dll.Get(MediaInfoLib::Stream_Audio, i, _T("BitRate/String"));
				const wstring sChannelPos = g_media_info_dll.Get(MediaInfoLib::Stream_Audio, i, _T("ChannelPositions"));
				const uint16_t iChannels = Util::toInt(g_media_info_dll.Get(MediaInfoLib::Stream_Audio, i, _T("Channel(s)")));
				const auto l_pos = sChannelPos.find(_T("LFE"), 0);
				std::string sChannels;
				if (l_pos != string::npos)
				{
					sChannels = Util::toString(iChannels - 1) + ".1";
				}
				else
				{
					sChannels = Util::toString(iChannels) + ".0";
				}
				
				const wstring sLanguage = g_media_info_dll.Get(MediaInfoLib::Stream_Audio, i, _T("Language/String1"));
				std::string audioFormatString;
				if (!sFormat.empty() || !sBitRate.empty() || !sChannels.empty() || !sLanguage.empty())
				{
					if (!sFormat.empty())
					{
						audioFormatString += ' ';
						audioFormatString += Text::fromT(sFormat);
						audioFormatString += ',';
					}
					if (!sChannels.empty())
					{
						audioFormatString += ' ';
						audioFormatString += sChannels;
						audioFormatString += ',';
					}
					if (!sBitRate.empty())
					{
						audioFormatString += ' ';
						audioFormatString += Text::fromT(sBitRate);
						audioFormatString += ',';
					}
					if (!sLanguage.empty())
					{
						audioFormatString += ' ';
						audioFormatString += Text::fromT(sLanguage);
						audioFormatString += ',';
					}
					if (!audioFormatString.empty() && audioFormatString[audioFormatString.length() - 1] == ',')
					{
						audioFormatString = audioFormatString.substr(0, audioFormatString.length() - 1); // Remove last ,
					}
					l_audio_dup_filter[audioFormatString]++;
				}
			}
			std::string l_audio_all;
			std::string l_sep;
			for (auto k = l_audio_dup_filter.cbegin(); k != l_audio_dup_filter.cend(); ++k)
			{
				l_audio_all += l_sep;
				if (k->second == 1)
					l_audio_all += k->first;
				else
					l_audio_all += k->first + " (x" + Util::toString(k->second) + ")";
				l_sep = " |";
			}
			wstring l_width;
			wstring l_height;
#ifdef USE_MEDIAINFO_IMAGES
			l_width = g_media_info_dll.Get(MediaInfoLib::Stream_Image, 0, _T("Width"));
			l_height = g_media_info_dll.Get(MediaInfoLib::Stream_Image, 0, _T("Height"));
			if (l_width.empty() && l_height.empty())
#endif
			{
				l_width = g_media_info_dll.Get(MediaInfoLib::Stream_Video, 0, _T("Width"));
				if (!l_width.empty())
					l_height = g_media_info_dll.Get(MediaInfoLib::Stream_Video, 0, _T("Height"));
			}
			p_media.m_mediaX = Util::toInt(l_width);
			if (p_media.m_mediaX)
				p_media.m_mediaY = Util::toInt(l_height);
			else
				p_media.m_mediaY = 0;
				
			const wstring sDuration = g_media_info_dll.Get(MediaInfoLib::Stream_General, 0, _T("Duration/String"));
			if (!sDuration.empty() || !l_audio_all.empty())
			{
				string audioGeneral;
				if (!sDuration.empty())
				{
					audioGeneral += Text::fromT(sDuration) + " |";
				}
				p_media.m_audio = audioGeneral;
				// No Duration => No sound
				if (!l_audio_all.empty())
				{
					p_media.m_audio += l_audio_all;
				}
			}
			
			const size_t videoCount =  g_media_info_dll.Count_Get(MediaInfoLib::Stream_Video);
			if (videoCount > 0)
			{
				string videoString;
				for (size_t i = 0; i < videoCount; i++)
				{
					wstring sVFormat = g_media_info_dll.Get(MediaInfoLib::Stream_Video, i, _T("Format"));
#if defined (SSA_REMOVE_NEEDLESS_WORDS_FROM_VIDEO_AUDIO_INFO)
					boost::replace_all(sVFormat, _T(" Video"), Util::emptyStringT);
					boost::replace_all(sVFormat, _T(" Visual"), Util::emptyStringT);
#endif
					wstring sVBitrate = g_media_info_dll.Get(MediaInfoLib::Stream_Video, i, _T("BitRate/String"));
					wstring sVFrameRate = g_media_info_dll.Get(MediaInfoLib::Stream_Video, i, _T("FrameRate/String"));
					if (!sVFormat.empty() || !sVBitrate.empty() || !sVFrameRate.empty())
					{
					
						if (!sVFormat.empty())
						{
							videoString += Text::fromT(sVFormat);
							videoString += ", ";
						}
						if (!sVBitrate.empty())
						{
							videoString += Text::fromT(sVBitrate);
							videoString += ", ";
						}
						if (!sVFrameRate.empty())
						{
							videoString += Text::fromT(sVFrameRate);
							videoString += ", ";
						}
						videoString = videoString.substr(0, videoString.length() - 2); // Remove last ,
						videoString += " | ";
					}
				}
				
				if (videoString.length() > 3) // This is false only in theorical way.
					p_media.m_video = videoString.substr(0, videoString.length() - 3); // Remove last |
			}
			g_media_info_dll.Close();
		}
		g_cur_mediainfo_file_tth.clear();
		g_cur_mediainfo_file.clear();
		return true;
	}
	catch (std::exception& e)
	{
		Util::setRegistryValueString(FLYLINKDC_REGISTRY_MEDIAINFO_CRASH_KEY, Text::toT(g_cur_mediainfo_file + " error: " + string(e.what())));
		LogManager::getInstance()->message("HashManager::getMediaInfo: " + p_name + "TTH:" + p_tth.toBase32() + ' ' + STRING(ERROR_STRING) + ": " + string(e.what()));
		char l_buf[4000];
		l_buf[0] = 0;
		sprintf_s(l_buf, _countof(l_buf), CSTRING(ERROR_MEDIAINFO_SCAN), p_name.c_str(), e.what());
		::MessageBox(0, Text::toT(l_buf).c_str(), _T("Error mediainfo!"), MB_ICONERROR);
		return false;
	}
	catch (...)
	{
		// TODO сюда не попадаем если SEH - найти способ и поправить
		Util::setRegistryValueString(FLYLINKDC_REGISTRY_MEDIAINFO_CRASH_KEY, Text::toT(g_cur_mediainfo_file + " catch(...) "));
		throw;
	}
}

bool HashManager::checkTTH(const string& fname, const string& fpath, int64_t p_path_id, int64_t aSize, int64_t aTimeStamp, TTHValue& p_out_tth)
{
	// Lock l(cs); [-] IRainman fix: no data to lock.
	const bool l_db = CFlylinkDBManager::getInstance()->checkTTH(fname, p_path_id, aSize, aTimeStamp, p_out_tth);
#ifdef IRAINMAN_NTFS_STREAM_TTH
	const string name = fpath + fname;
#endif // IRAINMAN_NTFS_STREAM_TTH
	if (!l_db)
	{
#ifdef IRAINMAN_NTFS_STREAM_TTH
		TigerTree l_TT;
		if (m_streamstore.loadTree(name, l_TT)) // [!] IRainman fix: no needs to lock.
		{
			addFileFromStream(name, l_TT, aSize); // [!] IRainman fix: no needs to lock.
		}
		else
		{
#endif // IRAINMAN_NTFS_STREAM_TTH      
			hasher.hashFile(name, aSize); // [!] IRainman fix: no needs to lock.
			return false;
			
#ifdef IRAINMAN_NTFS_STREAM_TTH
		}
#endif // IRAINMAN_NTFS_STREAM_TTH
	}
#ifdef IRAINMAN_NTFS_STREAM_TTH
	else
	{
		if (File::isExist(name))
		{
			TigerTree l_TT;
			if (CFlylinkDBManager::getInstance()->getTree(p_out_tth, l_TT)) // [!] IRainman fix: no needs to lock.
			{
				m_streamstore.saveTree(name, l_TT); // [!] IRainman fix: no needs to lock.
			}
		}
	}
#endif // IRAINMAN_NTFS_STREAM_TTH
	return true;
}

const TTHValue HashManager::getTTH(const string& fname, const string& fpath, int64_t aSize) throw(HashException)
{
	// Lock l(cs); [-] IRainman fix.
	TTHValue l_tth;
	bool l_is_find_tth = CFlylinkDBManager::getInstance()->findTTH(fname, fpath, l_tth); // [!] IRainman fix: no needs to lock.
	if (!l_is_find_tth)
	{
		const string name = fpath + fname;
#ifdef IRAINMAN_NTFS_STREAM_TTH
		TigerTree l_TT;
		if (m_streamstore.loadTree(name, l_TT)) // [!] IRainman fix: no needs to lock.
		{
			addFileFromStream(name, l_TT, aSize); // [!] IRainman fix: no needs to lock.
			l_is_find_tth = CFlylinkDBManager::getInstance()->findTTH(fname, fpath, l_tth);
			dcassert(l_is_find_tth);
			if (!l_is_find_tth)
			{
				throw HashException("Error HashManager::getTTH - CFlylinkDBManager::getInstance()->findTTH! - mail ppa74@ya.ru");
			}
		}
		else
		{
#endif // IRAINMAN_NTFS_STREAM_TTH
		
			hasher.hashFile(name , aSize); // [!] IRainman fix: no needs to lock.
			throw HashException(Util::emptyString);
			
#ifdef IRAINMAN_NTFS_STREAM_TTH
		}
		if (!BOOLSETTING(SAVE_TTH_IN_NTFS_FILESTREAM))
		{
			HashManager::getInstance()->m_streamstore.deleteStream(name); // [!] IRainman fix: no needs to lock.
		}
#endif // IRAINMAN_NTFS_STREAM_TTH
	}
	return l_tth;
}

void HashManager::hashDone(const string& aFileName, int64_t aTimeStamp, const TigerTree& tth, int64_t speed,
                           bool p_is_ntfs, int64_t p_size)
{
	// Lock l(cs); [-] IRainman fix: no data to lock.
	dcassert(!aFileName.empty());
	if (aFileName.empty())
	{
		LogManager::getInstance()->message("HashManager::hashDone - aFileName.empty()");
		return;
	}
	CFlyMediaInfo l_out_media;
	try
	{
		addFile(aFileName, aTimeStamp, tth, p_size, l_out_media);
#ifdef IRAINMAN_NTFS_STREAM_TTH
		if (BOOLSETTING(SAVE_TTH_IN_NTFS_FILESTREAM))
		{
			if (!p_is_ntfs) // [+] PPA TTH received from the NTFS stream, do not write back!
			{
				HashManager::getInstance()->m_streamstore.saveTree(aFileName, tth);
			}
		}
		else
		{
			HashManager::getInstance()->m_streamstore.deleteStream(aFileName);
		}
#endif // IRAINMAN_NTFS_STREAM_TTH
	}
	catch (const Exception& e)
	{
		LogManager::getInstance()->message(STRING(HASHING_FAILED) + ' ' + aFileName + e.getError());
		return;
	}
	fire(HashManagerListener::TTHDone(), aFileName, tth.getRoot(), aTimeStamp, l_out_media, p_size);
	CFlylinkDBManager::getInstance()->push_download_tth(tth.getRoot());
	
	string fn = aFileName;
	if (count(fn.begin(), fn.end(), PATH_SEPARATOR) >= 2)
	{
		string::size_type i = fn.rfind(PATH_SEPARATOR);
		i = fn.rfind(PATH_SEPARATOR, i - 1);
		fn.erase(0, i);
		fn.insert(0, "...");
	}
	if (speed > 0)
	{
		LogManager::getInstance()->message(STRING(HASHING_FINISHED) + ' ' + fn + " (" + Util::formatBytes(speed) + '/' + STRING(S) + ")");
	}
	else
	{
		LogManager::getInstance()->message(STRING(HASHING_FINISHED) + ' ' + fn);
	}
}

bool HashManager::addFile(const string& p_file_name, int64_t p_time_stamp, const TigerTree& p_tth, int64_t p_size, CFlyMediaInfo& p_out_media)
{
	const string l_name = Text::toLower(Util::getFileName(p_file_name));
	const string l_path = Text::toLower(Util::getFilePath(p_file_name));
	p_out_media.init(); // TODO - делается двойной инит
	int64_t l_path_id = 0;
	const int64_t l_tth_id = CFlylinkDBManager::getInstance()->merge_file(l_path, l_name, p_time_stamp, p_tth, false , l_path_id);
	if (getMediaInfo(p_file_name, p_out_media, p_size, p_tth.getRoot()))
	{
		return CFlylinkDBManager::getInstance()->merge_mediainfo(l_tth_id, l_path_id, l_name, p_out_media); // Если медиаинфу расчитали - скинем ее в базу
	}
	else
	{
		dcassert(l_tth_id != 0);
		return l_tth_id != 0;
	}
}

void HashManager::Hasher::hashFile(const string& fileName, int64_t size)
{
	FastLock l(cs);
	if (w.insert(make_pair(fileName, size)). second)
	{
		m_CurrentBytesLeft += size;// [+]IRainman
		if (paused > 0)
			paused++;
		else
			m_s.signal();
			
		int64_t bytesLeft;
		size_t filesLeft;
		getBytesAndFileLeft(bytesLeft, filesLeft);
		
		if (bytesLeft > iMaxBytes)
			iMaxBytes = bytesLeft;
			
		if (filesLeft > dwMaxFiles)
			dwMaxFiles = filesLeft;
	}
}

bool HashManager::Hasher::pause()
{
	FastLock l(cs);
	return paused++ > 0;
}

void HashManager::Hasher::resume()
{
	FastLock l(cs);
	while (--paused > 0)
	{
		m_s.signal();
	}
}

bool HashManager::Hasher::isPaused() const
{
	FastLock l(cs);
	return paused > 0;
}

void HashManager::Hasher::stopHashing(const string& baseDir)
{
	FastLock l(cs);
	if (baseDir.empty())
	{
		// [+]IRainman When user closes the program with a chosen operation "abort hashing"
		// in the hashing dialog then the hesher is cleaning.
		w.clear();
		m_CurrentBytesLeft = 0;
	}
	else
	{
		for (auto i = w.cbegin(); i != w.cend();)
		{
			if (strnicmp(baseDir, i->first, baseDir.length()) == 0)
			{
				m_CurrentBytesLeft -= i->second;// [+]IRainman
				w.erase(i++);
			}
			else
			{
				++i;
			}
		}
	}
	// [+] brain-ripper
	// cleanup state
	m_running = false;
	dwMaxFiles = 0;
	iMaxBytes = 0;
	m_fname.erase();
	currentSize = 0;
}

void HashManager::Hasher::instantPause()
{
	bool wait = false;
	{
		FastLock l(cs);
		if (paused > 0)
		{
			paused++;
			wait = true;
		}
	}
	if (wait)
	{
		m_s.wait();
	}
}

static const size_t g_HashBufferSize = 16 * 1024 * 1024;

#ifdef FLYLINKDC_HE

# ifdef _WIN32

static inline DWORD getpagesize() // [+] IRainman opt: for align hasher buffer in memory with respect pagesize for current architecture in Win32.
{
	SYSTEM_INFO si = { 0 };
	GetSystemInfo(&si);
	return max(si.dwPageSize, si.dwAllocationGranularity);
}

# endif // _WIN32

# ifdef _DEBUG

static inline size_t getHashBufferSize()
{
	const size_t l_PageSize = getpagesize();
	const size_t BUF_SIZE = g_HashBufferSize - (g_HashBufferSize % l_PageSize);
	const string l_message = "Page size on this system = " + Util::toString(l_PageSize) + " bytes.\nBUF_SIZE = " + Util::toString(BUF_SIZE) + " bytes.\n";
	dcdebug("%s", l_message.c_str());
	return BUF_SIZE;
}
static const size_t BUF_SIZE = getHashBufferSize();

# else // _DEBUG

static const size_t BUF_SIZE = g_HashBufferSize - (g_HashBufferSize % getpagesize());

# endif // _DEBUG

#else // FLYLINKDC_HE

static const size_t BUF_SIZE = g_HashBufferSize;

#endif // FLYLINKDC_HE

#ifdef _WIN32

bool HashManager::Hasher::fastHash(const string& fname, uint8_t* buf, TigerTree& tth, int64_t size)
{
	HANDLE h = INVALID_HANDLE_VALUE;
	DWORD l_sector_size = 0;
	DWORD l_tmp;
	BOOL l_sector_result;
	// TODO - размер сектора определять один раз для одной буквы через массив от A до Z и не звать GetDiskFreeSpaceA
	// на каждый файл
	// TODO - узнать зачем его вообще определять?
	if (fname.size() >= 3 && fname[1] == ':')
	{
		char l_drive[4];
		memcpy(l_drive, fname.c_str(), 3);
		l_drive[3] = 0;
		l_sector_result = GetDiskFreeSpaceA(l_drive, &l_tmp, &l_sector_size, &l_tmp, &l_tmp);
	}
	else
	{
		l_sector_result = GetDiskFreeSpace(Text::toT(Util::getFilePath(fname)).c_str(), &l_tmp, &l_sector_size, &l_tmp, &l_tmp);
	}
	if (!l_sector_result)
	{
		return false;
		// TODO Залогировать ошибку.
	}
	else
	{
		if ((BUF_SIZE % l_sector_size) != 0)
		{
			return false;
		}
		else
		{
			h = ::CreateFile(File::formatPath(Text::toT(fname)).c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
			                 FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);
			if (h == INVALID_HANDLE_VALUE)
				return false;
		}
	}
	DWORD hn = 0;
	DWORD rn = 0;
	uint8_t* hbuf = buf + BUF_SIZE;
	uint8_t* rbuf = buf;
	
	OVERLAPPED over = { 0 };
	BOOL res = TRUE;
	over.hEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
	
	bool ok = false;
	
	uint64_t lastRead = GET_TICK();
	if (!::ReadFile(h, hbuf, BUF_SIZE, &hn, &over))
	{
		if (GetLastError() == ERROR_HANDLE_EOF)
		{
			hn = 0;
		}
		else if (GetLastError() == ERROR_IO_PENDING)
		{
			if (!GetOverlappedResult(h, &over, &hn, TRUE))
			{
				if (GetLastError() == ERROR_HANDLE_EOF)
				{
					hn = 0;
				}
				else
				{
					goto cleanup;
				}
			}
		}
		else
		{
			goto cleanup;
		}
	}
	
	over.Offset = hn;
	size -= hn;
	
	// [+] brain-ripper
	// exit loop if "running" equals false.
	// "running" sets to false in stopHashing function
	while (!m_stop && m_running)
	{
		if (size > 0)
		{
			// Start a new overlapped read
			ResetEvent(over.hEvent);
			if (GetMaxHashSpeed() > 0)
			{
				const uint64_t now = GET_TICK();
				const uint64_t minTime = hn * 1000LL / (GetMaxHashSpeed() * 1024LL * 1024LL);
				if (lastRead + minTime > now)
				{
					const uint64_t diff = now - lastRead;
					sleep(minTime - diff);
				}
				lastRead = lastRead + minTime;
			}
			else
			{
				lastRead = GET_TICK();
			}
			res = ReadFile(h, rbuf, BUF_SIZE, &rn, &over);
		}
		else
		{
			rn = 0;
		}
		
		tth.update(hbuf, hn);
		
		{
			FastLock l(cs);
			currentSize = max(currentSize - hn, _LL(0));
		}
		
		if (size == 0)
		{
			ok = true;
			break;
		}
		
		if (!res)
		{
			// deal with the error code
			switch (GetLastError())
			{
				case ERROR_IO_PENDING:
					if (!GetOverlappedResult(h, &over, &rn, TRUE))
					{
						dcdebug("Error 0x%x: %s\n", GetLastError(), Util::translateError(GetLastError()).c_str());
						goto cleanup;
					}
					break;
				default:
					dcdebug("Error 0x%x: %s\n", GetLastError(), Util::translateError(GetLastError()).c_str());
					goto cleanup;
			}
		}
		
		instantPause();
		
		*((uint64_t*)&over.Offset) += rn;
		size -= rn;
		
		std::swap(rbuf, hbuf);
		std::swap(rn, hn);
	}
	
cleanup:
	::CloseHandle(over.hEvent);
	::CloseHandle(h);
	return ok;
}

#else // !_WIN32

bool HashManager::Hasher::fastHash(const string& filename, uint8_t* , TigerTree& tth, int64_t size)
{
	int fd = open(Text::fromUtf8(filename).c_str(), O_RDONLY);
	if (fd == -1)
	{
		dcdebug("Error opening file %s: %s\n", filename.c_str(), Util::translateError(errno).c_str());
		return false;
	}

	int64_t size_left = size;
	int64_t pos = 0;
	int64_t size_read = 0;
	void *buf = 0;
	bool ok = false;

	uint64_t lastRead = GET_TICK();
	while (pos <= size && !stop)
	{
		if (size_left > 0)
		{
			size_read = std::min(size_left, BUF_SIZE);
			buf = mmap(0, size_read, PROT_READ, MAP_SHARED, fd, pos);
			if (buf == MAP_FAILED)
			{
				dcdebug("Error calling mmap for file %s: %s\n", filename.c_str(), Util::translateError(errno).c_str());
				break;
			}

			if (posix_madvise(buf, size_read, POSIX_MADV_SEQUENTIAL | POSIX_MADV_WILLNEED) == -1)
			{
				dcdebug("Error calling madvise for file %s: %s\n", filename.c_str(), Util::translateError(errno).c_str());
				break;
			}

			if (GetMaxHashSpeed() > 0)
			{
				const uint64_t now = GET_TICK();
				const uint64_t minTime = size_read * 1000LL / (GetMaxHashSpeed() * 1024LL * 1024LL);
				if (lastRead + minTime > now)
				{
					const uint64_t diff = now - lastRead;
					sleep(minTime - diff);
				}
				lastRead = lastRead + minTime;
			}
			else
			{
				lastRead = GET_TICK();
			}
		}
		else
		{
			size_read = 0;
		}

		tth.update(buf, size_read);

		{
			FastLock l(cs);
			currentSize = max(static_cast<uint64_t>(currentSize - size_read), static_cast<uint64_t>(0));
		}

		if (size_left == 0)
		{
			ok = true;
			break;
		}

		instantPause();

		if (munmap(buf, size_read) == -1)
		{
			dcdebug("Error calling munmap for file %s: %s\n", filename.c_str(), Util::translateError(errno).c_str());
			break;
		}
		pos += size_read;
		size_left -= size_read;
	}
	close(fd);
	return ok;
}

#endif // !_WIN32

int HashManager::Hasher::run()
{
	setThreadPriority(Thread::IDLE);
	
	uint8_t* buf = nullptr;
	bool virtualBuf = true;
	
	bool last = false;
	for (;;)
	{
		m_s.wait();
		if (m_stop)
			break;
		if (m_rebuild)
		{
			HashManager::getInstance()->doRebuild();
			m_rebuild = false;
			LogManager::getInstance()->message(STRING(HASH_REBUILT));
			continue;
		}
		{
			FastLock l(cs);
			if (!w.empty())
			{
				m_fname = w.begin()->first;
				currentSize = w.begin()->second;
				m_CurrentBytesLeft -= currentSize;// [+]IRainman
				w.erase(w.begin());
				last = w.empty();
				if (!m_running)
				{
					uiStartTime = GET_TICK();
					m_running = true;
				}
			}
			else
			{
				last = true;
				m_fname.clear();
				m_running = false;
				iMaxBytes = 0;
				dwMaxFiles = 0;
				m_CurrentBytesLeft = 0;// [+]IRainman
			}
		}
		// [-] dcassert(!m_fname.empty()); [-] IRainman fix: normal behavior when you close the program while hashing, and the forced interruption of the process.
		if (!m_fname.empty())
		{
			int64_t l_size = 0;
			int64_t l_outFiletime = 0;
			File::isExist(m_fname, l_size, l_outFiletime);
			int64_t l_sizeLeft = l_size;
#ifdef _WIN32
			if (buf == NULL)
			{
				virtualBuf = true;
				buf = (uint8_t*)VirtualAlloc(NULL, 2 * BUF_SIZE, MEM_COMMIT, PAGE_READWRITE);
			}
#endif
			if (buf == NULL)
			{
				virtualBuf = false;
				buf = new uint8_t[BUF_SIZE]; // bad_alloc! https://www.box.net/shared/d07faa588d5f44d577a0
			}
			try
			{
				const int64_t bs = TigerTree::getMaxBlockSize(l_size);
				const uint64_t start = GET_TICK();
				const int64_t timestamp = l_outFiletime;
				int64_t speed = 0;
				size_t n = 0;
				TigerTree fastTTH(bs);
				TigerTree slowTTH(bs);
				TigerTree* tth = &fastTTH;
				bool l_is_ntfs = false;
#ifdef IRAINMAN_NTFS_STREAM_TTH
				if (l_size > 0 && HashManager::getInstance()->m_streamstore.loadTree(m_fname, fastTTH, l_size)) //[+]IRainman
				{
					l_is_ntfs = true; //[+]PPA
					LogManager::getInstance()->message(STRING(LOAD_TTH_FROM_NTFS) + ' ' + m_fname); //[!]NightOrion(translate)
				}
#endif
#ifdef _WIN32
#ifdef IRAINMAN_NTFS_STREAM_TTH
				if (!l_is_ntfs)
				{
#endif
					if (!virtualBuf || !BOOLSETTING(FAST_HASH) || !fastHash(m_fname, buf, fastTTH, l_size))
					{
#else
				if (!BOOLSETTING(FAST_HASH) || !fastHash(fname, 0, fastTTH, l_size))
				{
#endif
						// [+] brain-ripper
						if (m_running)
						{
							tth = &slowTTH;
							uint64_t lastRead = GET_TICK();
							File l_slow_file_reader(m_fname, File::READ, File::OPEN);
							do
							{
								size_t bufSize = BUF_SIZE;
								
								if (GetMaxHashSpeed() > 0) // [+] brain-ripper
								{
									const uint64_t now = GET_TICK();
									const uint64_t minTime = n * 1000LL / (GetMaxHashSpeed() * 1024LL * 1024LL);
									if (lastRead + minTime > now)
									{
										sleep(minTime - (now - lastRead));
									}
									lastRead = lastRead + minTime;
								}
								else
								{
									lastRead = GET_TICK();
								}
								n = l_slow_file_reader.read(buf, bufSize);
								if (n > 0) // [+]PPA
								{
									tth->update(buf, n);
									{
										FastLock l(cs);
										currentSize = max(static_cast<uint64_t>(currentSize - n), static_cast<uint64_t>(0)); // TODO - max от 0 для беззнакового?
									}
									l_sizeLeft -= n;
									
									instantPause();
								}
							}
							while (!m_stop && m_running && n > 0);
						}
						else
							tth = nullptr;
					}
					else
					{
						l_sizeLeft = 0; // Variable 'l_sizeLeft' is assigned a value that is never used.
					}
#ifdef IRAINMAN_NTFS_STREAM_TTH
				}
#endif
				const uint64_t end = GET_TICK();
				if (end > start) // TODO: Why is not possible?
				{
					speed = l_size * _LL(1000) / (end - start);
				}
				if (m_running)
				{
#ifdef IRAINMAN_NTFS_STREAM_TTH
					if (l_is_ntfs)
					{
						HashManager::getInstance()->hashDone(m_fname, timestamp, *tth, speed, l_is_ntfs, l_size);
					}
					else
#endif
						if (tth)
						{
							tth->finalize();
							HashManager::getInstance()->hashDone(m_fname, timestamp, *tth, speed, l_is_ntfs, l_size);
						}
				}
			}
			catch (const FileException& e)
			{
				LogManager::getInstance()->message(STRING(ERROR_HASHING) + ' ' + m_fname + ": " + e.getError());
			}
		}
		{
			FastLock l(cs);
			m_fname.clear();
			currentSize = 0;
			
			if (w.empty())
			{
				m_running = false;
				iMaxBytes = 0;
				dwMaxFiles = 0;
				m_CurrentBytesLeft = 0;//[+]IRainman
			}
		}
		
		if (buf != NULL && (last || m_stop))
		{
			if (virtualBuf)
				VirtualFree(buf, 0, MEM_RELEASE);
			else
				delete [] buf;
			buf = nullptr;
		}
	}
	return 0;
}

HashManager::HashPauser::HashPauser()
{
	resume = !HashManager::getInstance()->pauseHashing();
}

HashManager::HashPauser::~HashPauser()
{
	if (resume)
		HashManager::getInstance()->resumeHashing();
}

bool HashManager::pauseHashing()
{
	return hasher.pause();
}

void HashManager::resumeHashing()
{
	hasher.resume();
}

bool HashManager::isHashingPaused() const
{
	return hasher.isPaused();
}

/**
 * @file
 * $Id: HashManager.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
