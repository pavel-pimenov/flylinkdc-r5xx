
#include "stdinc.h"

#ifdef IRAINMAN_INCLUDE_OLD_CLIENT_PROFILE_MANAGER
#include "ClientProfileManager.h"
#include "SimpleXML.h"

void ClientProfileManager::saveClientProfiles()
{
	try
	{
		SimpleXML xml;
		xml.addTag("Profiles");
		xml.stepIn();
		
		xml.addTag("ClientProfilesV2");
		xml.stepIn();
		for (auto l = clientProfiles.cbegin(); l != clientProfiles.cend(); ++l)
		{
			xml.addTag("ClientProfile");
			xml.stepIn();
			xml.addTag("Name", l->getName());
			xml.addTag("Version", l->getVersion());
			xml.addTag("Tag", l->getTag());
			xml.addTag("ExtendedTag", l->getExtendedTag());
#ifdef IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY
			xml.addTag("Lock", l->getLock());
			xml.addTag("Pk", l->getPk());
#endif
			xml.addTag("Supports", l->getSupports());
			//xml.addTag("TestSUR", l->getTestSUR());
			xml.addTag("UserConCom", l->getUserConCom());
			xml.addTag("Status", l->getStatus());
			xml.addTag("CheatingDescription", l->getCheatingDescription());
			xml.addTag("RawToSend", Util::toString(l->getRawToSend()));
			//xml.addTag("TagVersion", Util::toString(l->getTagVersion()));
			xml.addTag("UseExtraVersion", Util::toString(l->getUseExtraVersion()));
			xml.addTag("CheckMismatch", Util::toString(l->getCheckMismatch()));
			xml.addTag("Connection", l->getConnection());
			xml.addTag("Comment", l->getComment());
			xml.addTag("Recheck", l->getRecheck());
			xml.addTag("SkipExtended", l->getSkipExtended());
			xml.stepOut();
		}
		xml.stepOut();
		
		xml.addTag("Params");
		xml.stepIn();
		for (auto m = params.cbegin(); m != params.cend(); ++m)
		{
			xml.addTag("Param");
			xml.addChildAttrib("Name", m->first);
			xml.addChildAttrib("RegExp", m->second);
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
		dcdebug("FavoriteManager::saveClientProfiles: %s\n", e.getError().c_str());
	}
}


void ClientProfileManager::loadClientProfiles(SimpleXML* aXml)
{
	StringList sl;
	aXml->resetCurrentChild();
	if (aXml->findChild("ClientProfilesV2"))
	{
		aXml->stepIn();
		while (aXml->findChild("ClientProfile"))
		{
			aXml->stepIn();
			if (aXml->findChild("Name"))
			{
				sl.push_back(aXml->getChildData());
			}
			else
			{
				sl.push_back(Util::emptyString);
			}
			if (aXml->findChild("Version"))
			{
				sl.push_back(aXml->getChildData());
			}
			else
			{
				sl.push_back(Util::emptyString);
			}
			if (aXml->findChild("Tag"))
			{
				sl.push_back(aXml->getChildData());
			}
			else
			{
				sl.push_back(Util::emptyString);
			}
			if (aXml->findChild("ExtendedTag"))
			{
				sl.push_back(aXml->getChildData());
			}
			else
			{
				sl.push_back(Util::emptyString);
			}
#ifdef IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY
			if (aXml->findChild("Lock"))
			{
				sl.push_back(aXml->getChildData());
			}
			else
			{
				sl.push_back(Util::emptyString);
			}
			if (aXml->findChild("Pk"))
			{
				sl.push_back(aXml->getChildData());
			}
			else
			{
				sl.push_back(Util::emptyString);
			}
#endif // IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY
			if (aXml->findChild("Supports"))
			{
				sl.push_back(aXml->getChildData());
			}
			else
			{
				sl.push_back(Util::emptyString);
			}
			/*
			if (aXml->findChild("TestSUR"))
			{
			    sl.push_back(aXml->getChildData());
			}
			else
			{
			    sl.push_back(Util::emptyString);
			}
			*/
			if (aXml->findChild("UserConCom"))
			{
				sl.push_back(aXml->getChildData());
			}
			else
			{
				sl.push_back(Util::emptyString);
			}
			if (aXml->findChild("Status"))
			{
				sl.push_back(aXml->getChildData());
			}
			else
			{
				sl.push_back(Util::emptyString);
			}
			if (aXml->findChild("CheatingDescription"))
			{
				sl.push_back(aXml->getChildData());
			}
			else
			{
				sl.push_back(Util::emptyString);
			}
			if (aXml->findChild("RawToSend"))
			{
				sl.push_back(aXml->getChildData());
			}
			else
			{
				sl.push_back(Util::emptyString);
			}
			//if(aXml->findChild("TagVersion"))         { sl.push_back(aXml->getChildData()); } else { sl.push_back(Util::emptyString); }
			if (aXml->findChild("UseExtraVersion"))
			{
				sl.push_back(aXml->getChildData());
			}
			else
			{
				sl.push_back(Util::emptyString);
			}
			if (aXml->findChild("CheckMismatch"))
			{
				sl.push_back(aXml->getChildData());
			}
			else
			{
				sl.push_back(Util::emptyString);
			}
			if (aXml->findChild("Connection"))
			{
				sl.push_back(aXml->getChildData());
			}
			else
			{
				sl.push_back(Util::emptyString);
			}
			if (aXml->findChild("Comment"))
			{
				sl.push_back(aXml->getChildData());
			}
			else
			{
				sl.push_back(Util::emptyString);
			}
			if (aXml->findChild("Recheck"))
			{
				sl.push_back(aXml->getChildData());
			}
			else
			{
				sl.push_back(Util::emptyString);
			}
			if (aXml->findChild("SkipExtended"))
			{
				sl.push_back(aXml->getChildData());
			}
			else
			{
				sl.push_back(Util::emptyString);
			}
			
			addClientProfile(sl[0], sl[1], sl[2], sl[3], sl[4], sl[5], sl[6], sl[7], sl[8], sl[9], sl[10], Util::toInt(sl[11]),
			                 Util::toInt(sl[12]), Util::toInt(sl[13]), /*Util::toInt(sl[14]),*/ sl[14], sl[15], Util::toInt(sl[16]), Util::toInt(sl[17])); // TODO: fix me
			sl.clear();
			aXml->stepOut();
		}
		aXml->stepOut();
	}
	aXml->resetCurrentChild();
	if (aXml->findChild("Params"))
	{
		aXml->stepIn();
		while (aXml->findChild("Param"))
		{
			params[aXml->getChildAttrib("Name")] = aXml->getChildAttrib("RegExp");
		}
		aXml->stepOut();
	}
}

void ClientProfileManager::loadClientProfiles()
{
	try
	{
		SimpleXML xml;
		xml.fromXML(File(Util::getConfigPath() + "Profiles.xml", File::READ, File::OPEN).read());
		
		if (xml.findChild("Profiles"))
		{
			xml.stepIn();
			loadClientProfiles(&xml);
			xml.stepOut();
		}
	}
	catch (const Exception& e)
	{
		dcdebug("FavoriteManager::loadClientProfiles: %s\n", e.getError().c_str());
	}
}

#endif // IRAINMAN_INCLUDE_OLD_CLIENT_PROFILE_MANAGER