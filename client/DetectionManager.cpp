/*
 * Copyright (C) 2007-2013 adrian_007, adrian-007 on o2 point pl
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

#ifdef IRAINMAN_INCLUDE_DETECTION_MANAGER

#include "DetectionManager.h"
#include "File.h"
#include "FilteredFile.h"
#include "BZUtils.h"

void DetectionManager::load()
{
	try
	{
		Util::migrate(Util::getConfigPath() + "Profiles.xml");
		
		SimpleXML xml;
		xml.fromXML(File(Util::getConfigPath() + "Profiles.xml", File::READ, File::OPEN).read());
		
		if (xml.findChild("Profiles"))
		{
			xml.stepIn();
			if (xml.findChild("ClientProfilesV3"))
			{
				xml.stepIn();
				while (xml.findChild("DetectionProfile"))
				{
					xml.stepIn();
					if (xml.findChild("DetectionEntry"))
					{
						uint32_t curId = Util::toUInt32(xml.getChildAttrib("ProfileID", Util::toString(++lastId)));
						if (curId < 1) continue;
						xml.stepIn();
						
						DetectionEntry item;
						lastId = std::max(curId, lastId);
						item.Id = curId;
						
						if (xml.findChild("Name"))
						{
							item.name = xml.getChildData();
							xml.resetCurrentChild();
						}
						if (xml.findChild("Cheat"))
						{
							item.cheat = xml.getChildData();
							xml.resetCurrentChild();
						}
						if (xml.findChild("Comment"))
						{
							item.comment = xml.getChildData();
							xml.resetCurrentChild();
						}
						if (xml.findChild("RawToSend"))
						{
							item.rawToSend = Util::toUInt32(xml.getChildData());
							xml.resetCurrentChild();
						}
						if (xml.findChild("ClientFlag"))
						{
							item.clientFlag = Util::toUInt32(xml.getChildData());
							xml.resetCurrentChild();
						}
						if (xml.findChild("IsEnabled"))
						{
							item.isEnabled = (Util::toInt(xml.getChildData()) > 0);
							xml.resetCurrentChild();
						}
						if (xml.findChild("CheckMismatch"))
						{
							item.checkMismatch = (Util::toInt(xml.getChildData()) > 0);
							xml.resetCurrentChild();
						}
						
						if (xml.findChild("INFMaps"))
						{
							xml.stepIn();
							while (xml.findChild("InfField"))
							{
								const string& field = xml.getChildAttrib("Field");
								const string& pattern = xml.getChildAttrib("Pattern");
								const string& type = xml.getChildAttrib("Protocol", "both");
								if (field.empty() || pattern.empty())
									continue;
								if (type == "both")
									item.defaultMap.push_back(make_pair(field, pattern));
								else if (type == "nmdc")
									item.nmdcMap.push_back(make_pair(field, pattern));
								else if (type == "adc")
									item.adcMap.push_back(make_pair(field, pattern));
							}
							xml.stepOut();
							xml.resetCurrentChild();
						}
						try
						{
							addDetectionItem(item);
						}
						catch (const Exception&)
						{
							//...
						}
						xml.stepOut();
					}
					xml.stepOut();
				}
				xml.stepOut();
			}
			else
			{
				importProfiles(xml);
			}
			xml.resetCurrentChild();
			if (xml.findChild("Params"))
			{
				xml.stepIn();
				while (xml.findChild("Param"))
				{
					const string& name = xml.getChildAttrib("Name");
					const string& pattern = xml.getChildAttrib("Pattern", xml.getChildAttrib("RegExp"));
					if (!name.empty() && !pattern.empty())
						params.insert(make_pair(name, pattern));
				}
				xml.stepOut();
			}
			xml.resetCurrentChild();
			if (xml.findChild("ProfileInfo"))
			{
				xml.stepIn();
				if (xml.findChild("DetectionProfile"))
				{
					xml.stepIn();
					if (xml.findChild("Version"))
					{
						setProfileVersion(xml.getChildData());
						xml.resetCurrentChild();
					}
					if (xml.findChild("Message"))
					{
						setProfileMessage(xml.getChildData());
						xml.resetCurrentChild();
					}
					if (xml.findChild("URL"))
					{
						setProfileUrl(xml.getChildData());
						xml.resetCurrentChild();
					}
					xml.stepOut();
				}
				xml.stepOut();
			}
			xml.stepOut();
		}
	}
	catch (const Exception& e)
	{
		dcdebug("DetectionManager::load: %s\n", e.getError().c_str());
	}
}

const DetectionManager::DetectionItems& DetectionManager::reload()
{
	Lock l(cs);
	det.clear();
	params.clear();
	load();
	
	return det;
}

const DetectionManager::DetectionItems& DetectionManager::reloadFromHttp(bool bz2 /*= false*/)
{
	Lock l(cs);
	if (bz2)
		loadCompressedProfiles();
		
	DetectionManager::DetectionItems oldDet;
	swap(det, oldDet);
	params.clear();
	load();
	for (auto j = det.begin(); j != det.end(); ++j)
	{
		for (auto k = oldDet.cbegin(); k != oldDet.cend(); ++k)
		{
			if (k->Id == j->Id)
			{
				j->rawToSend = k->rawToSend;
				j->clientFlag = k->clientFlag;
				j->isEnabled = k->isEnabled;
			}
		}
	}
	
	return det;
}

void DetectionManager::importProfiles(SimpleXML& xml)
{
	try
	{
		xml.resetCurrentChild();
		if (xml.findChild("ClientProfilesV2"))
		{
			xml.stepIn();
			
			while (xml.findChild("ClientProfile"))
			{
				xml.stepIn();
				string::size_type i;
				DetectionEntry item;
				
				item.Id = ++lastId;
				if (xml.findChild("Name"))
				{
					item.name = xml.getChildData();
					xml.resetCurrentChild();
				}
				if (xml.findChild("Version") && !xml.getChildData().empty())
				{
					item.nmdcMap.push_back(make_pair("VE", xml.getChildData()));
					xml.resetCurrentChild();
				}
				if (xml.findChild("Tag") && !xml.getChildData().empty())
				{
					string tagExp = xml.getChildData();
					i = xml.getChildData().find("%[version]");
					if (i != string::npos)
					{
						tagExp.replace(i, 10, "%[VE]");
					}
					
					item.nmdcMap.push_back(make_pair("TA", tagExp));
					xml.resetCurrentChild();
				}
				if (xml.findChild("ExtendedTag") && !xml.getChildData().empty())
				{
					string extTagExp = xml.getChildData();
					i = xml.getChildData().find("%[version2]");
					if (i != string::npos)
					{
						extTagExp.replace(i, 11, "[\\w\\.\\s]{2,10}");
					}
					
					item.nmdcMap.push_back(make_pair("DE", extTagExp));
					xml.resetCurrentChild();
				}
#ifdef IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY
				if (xml.findChild("Lock") && !xml.getChildData().empty())
				{
					item.nmdcMap.push_back(make_pair("LO", xml.getChildData()));
					xml.resetCurrentChild();
				}
				if (xml.findChild("Pk") && !xml.getChildData().empty())
				{
					string pkExp = xml.getChildData();
					i = xml.getChildData().find("%[version]");
					if (i != string::npos)
					{
						pkExp.replace(i, 10, "%[PKVE]");
					}
					
					item.nmdcMap.push_back(make_pair("PK", pkExp));
					xml.resetCurrentChild();
				}
#endif // IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY
				if (xml.findChild("Supports") && !xml.getChildData().empty())
				{
					item.nmdcMap.push_back(make_pair("SU", xml.getChildData()));
					xml.resetCurrentChild();
				}
				if (xml.findChild("UserConCom") && !xml.getChildData().empty())
				{
					item.nmdcMap.push_back(make_pair("UC", xml.getChildData()));
					xml.resetCurrentChild();
				}
				if (xml.findChild("Status") && !xml.getChildData().empty())
				{
					item.nmdcMap.push_back(make_pair("ST", xml.getChildData()));
					xml.resetCurrentChild();
				}
				if (xml.findChild("CheatingDescription"))
				{
					if (!xml.getChildData().empty())
					{
						item.clientFlag = DetectionEntry::RED;
					}
					
					item.cheat = xml.getChildData();
					xml.resetCurrentChild();
				}
				if (xml.findChild("RawToSend"))
				{
					item.rawToSend = Util::toUInt32(xml.getChildData());
					xml.resetCurrentChild();
				}
				if (xml.findChild("CheckMismatch"))
				{
					item.checkMismatch = (Util::toInt(xml.getChildData()) > 0);
					xml.resetCurrentChild();
				}
				if (xml.findChild("Connection") && !xml.getChildData().empty())
				{
					item.nmdcMap.push_back(make_pair("CO", xml.getChildData()));
					xml.resetCurrentChild();
				}
				if (xml.findChild("Comment"))
				{
					item.comment = xml.getChildData();
					xml.resetCurrentChild();
				}
				xml.stepOut();
				
				try
				{
					addDetectionItem(item);
				}
				catch (const Exception&)
				{
					//...
				}
			}
			xml.stepOut();
		}
	}
	catch (const Exception& e)
	{
		dcdebug("DetectionManager::importProfiles: %s\n", e.getError().c_str());
	}
}

void DetectionManager::save()
{
	try
	{
		SimpleXML xml;
		xml.addTag("Profiles");
		xml.stepIn();
		
		xml.addTag("ClientProfilesV3");
		xml.stepIn();
		
		Lock l(cs);
		for (auto i = det.cbegin(); i != det.cend(); ++i)
		{
			xml.addTag("DetectionProfile");
			xml.stepIn();
			{
				xml.addTag("DetectionEntry");
				xml.addChildAttrib("ProfileID", i->Id);
				xml.stepIn();
				{
					xml.addTag("Name", i->name);
					xml.addTag("Cheat", i->cheat);
					xml.addTag("Comment", i->comment);
					xml.addTag("RawToSend", Util::toString(i->rawToSend));
					xml.addTag("ClientFlag", Util::toString(i->clientFlag));
					xml.addTag("IsEnabled", i->isEnabled);
					xml.addTag("CheckMismatch", i->checkMismatch);
					
					xml.addTag("INFMaps");
					xml.stepIn();
					{
						const DetectionEntry::INFMap& InfMap = i->defaultMap;
						for (auto j = InfMap.cbegin(); j != InfMap.cend(); ++j)
						{
							xml.addTag("InfField");
							xml.addChildAttrib("Field", j->first);
							xml.addChildAttrib("Pattern", j->second);
							xml.addChildAttrib("Protocol", string("both"));
						}
					}
					{
						const DetectionEntry::INFMap& InfMap = i->nmdcMap;
						for (auto j = InfMap.cbegin(); j != InfMap.cend(); ++j)
						{
							xml.addTag("InfField");
							xml.addChildAttrib("Field", j->first);
							xml.addChildAttrib("Pattern", j->second);
							xml.addChildAttrib("Protocol", string("nmdc"));
						}
					}
					{
						const DetectionEntry::INFMap& InfMap = i->adcMap;
						for (auto j = InfMap.cbegin(); j != InfMap.cend(); ++j)
						{
							xml.addTag("InfField");
							xml.addChildAttrib("Field", j->first);
							xml.addChildAttrib("Pattern", j->second);
							xml.addChildAttrib("Protocol", string("adc"));
						}
					}
					xml.stepOut();
				}
				xml.stepOut();
			}
			xml.stepOut();
		}
		xml.stepOut();
		xml.addTag("Params");
		xml.stepIn();
		{
			for (auto j = params.cbegin(); j != params.cend(); ++j)
			{
				xml.addTag("Param");
				xml.addChildAttrib("Name", j->first);
				xml.addChildAttrib("Pattern", j->second);
			}
		}
		xml.stepOut();
		xml.addTag("ProfileInfo");
		xml.stepIn();
		{
			xml.addTag("DetectionProfile");
			xml.stepIn();
			{
				xml.addTag("Version", getProfileVersion());
				xml.addTag("Message", getProfileMessage());
				xml.addTag("URL", getProfileUrl());
			}
			xml.stepOut();
		}
		xml.stepOut();
		xml.stepOut();
		
		string fname = Util::getConfigPath() + "Profiles.xml";
		
		File f(fname + ".tmp", File::WRITE, File::CREATE | File::TRUNCATE);
		f.write(SimpleXML::utf8Header);
		f.write(xml.toXML());
		f.close();
		File::deleteFile(fname);
		File::renameFile(fname + ".tmp", fname);
		
	}
	catch (const Exception& e)
	{
		dcdebug("DetectionManager::save: %s\n", e.getError().c_str());
	}
}

void DetectionManager::loadCompressedProfiles()
{
	string xml;
	string file = Util::getConfigPath() + "Profiles.xml";
	
	Util::migrate(file + ".bz2");
	
	if (!File::isExist(file + ".bz2"))
		return;
	try
	{
		File ff(file + ".bz2", File::READ, File::OPEN);
		FilteredInputStream<UnBZFilter, false> f(&ff);
		const size_t BUF_SIZE = 64 * 1024;
		std::unique_ptr<char[]> buf(new char[BUF_SIZE]);
		size_t len;
		for (;;)
		{
			size_t n = BUF_SIZE;
			len = f.read(&buf[0], n);
			xml.append(&buf[0], len);
			if (len < BUF_SIZE)
				break;
		}
		
		File newfile(file + ".tmp", File::WRITE, File::CREATE | File::TRUNCATE);
		newfile.write(xml);
		newfile.close();
		File::deleteFile(file);
		//File::deleteFile(file + ".bz2");
		File::renameFile(file + ".tmp", file);
	}
	catch (...)
	{
		//
	}
}

void DetectionManager::addDetectionItem(DetectionEntry& e)
{
	Lock l(cs);
	if (det.size() >= 2147483647)
		throw Exception("No more items can be added!");
		
	validateItem(e, true);
	
	if (e.Id == 0)
	{
		e.Id = ++lastId;
		
		// This should only happen if lastId (aka. unsigned int) goes over it's capacity ie. virtually never :P
		while (e.Id == 0)
		{
			e.Id = Util::rand(1, 2147483647);
			for (auto i = det.cbegin(); i != det.cend(); ++i)
			{
				if (i->Id == e.Id)
				{
					e.Id = 0;
				}
			}
		}
	}
	
	det.push_back(e);
}

void DetectionManager::validateItem(const DetectionEntry& e, bool checkIds)
{
	Lock l(cs);
	if (checkIds && e.Id > 0)
	{
		for (auto i = det.cbegin(); i != det.cend(); ++i)
		{
			if (i->Id == e.Id || e.Id <= 0)
			{
				throw Exception("Item with this ID already exist!");
			}
		}
	}
	
	if (e.defaultMap.empty() && e.adcMap.empty() && e.nmdcMap.empty())
		throw Exception("You have to fill at least one map (Both, ADC or NMDC protocol)");
		
	{
		const DetectionEntry::INFMap& inf = e.defaultMap;
		for (auto i = inf.cbegin(); i != inf.cend(); ++i)
		{
			if (i->first.empty())
				throw Exception("INF entry name can't be empty!");
			else if (i->second.empty())
				throw Exception("INF entry pattern can't be empty!");
		}
	}
	{
		const DetectionEntry::INFMap& inf = e.nmdcMap;
		for (auto i = inf.cbegin(); i != inf.cend(); ++i)
		{
			if (i->first.empty())
				throw Exception("INF entry name can't be empty!");
			else if (i->second.empty())
				throw Exception("INF entry pattern can't be empty!");
		}
	}
	{
		const DetectionEntry::INFMap& inf = e.adcMap;
		for (auto i = inf.cbegin(); i != inf.cend(); ++i)
		{
			if (i->first.empty())
				throw Exception("INF entry name can't be empty!");
			else if (i->second.empty())
				throw Exception("INF entry pattern can't be empty!");
		}
	}
	
	if (e.name.empty()) throw Exception("Item's name can't be empty!");
}

void DetectionManager::removeDetectionItem(const uint32_t id) noexcept
{
	Lock l(cs);
	for (auto i = det.cbegin(); i != det.cend(); ++i)
	{
		if (i->Id == id)
		{
			det.erase(i);
			return;
		}
	}
}

void DetectionManager::updateDetectionItem(const uint32_t aOrigId, const DetectionEntry& e)
{
	Lock l(cs);
	validateItem(e, e.Id != aOrigId);
	for (auto i = det.begin(); i != det.end(); ++i)
	{
		if (i->Id == aOrigId)
		{
			*i = e;
			break;
		}
	}
}

bool DetectionManager::getDetectionItem(const uint32_t aId, DetectionEntry& e) noexcept
{
	Lock l(cs);
	for (auto i = det.cbegin(); i != det.cend(); ++i)
	{
		if (i->Id == aId)
		{
			e = *i;
			return true;
		}
	}
	return false;
}

bool DetectionManager::getNextDetectionItem(const uint32_t aId, int pos, DetectionEntry& e) noexcept
{
	Lock l(cs);
	for (auto i = det.cbegin(); i != det.cend(); ++i)
	{
		if (i->Id == aId)
		{
			i += pos;
			if (i < det.end() && i >= det.begin())
			{
				e = *i;
				return true;
			}
			return false;
		}
	}
	return false;
}

bool DetectionManager::moveDetectionItem(const uint32_t aId, int pos)
{
	Lock l(cs);
	for (auto i = det.begin(); i != det.end(); ++i)
	{
		if (i->Id == aId)
		{
			swap(*i, *(i + pos));
			return true;
		}
	}
	return false;
}

void DetectionManager::setItemEnabled(const uint32_t aId, bool enabled) noexcept
{
	Lock l(cs);
	for (auto i = det.begin(); i != det.end(); ++i)
	{
		if (i->Id == aId)
		{
			i->isEnabled = enabled;
			break;
		}
	}
}

#endif // IRAINMAN_INCLUDE_DETECTION_MANAGER

/**
 * @file
 * $Id: DetectionManager.cpp 87 2008-07-04 23:06:58Z adrian_007 $
 */
