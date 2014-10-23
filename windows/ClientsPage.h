#ifndef CLIENTSPAGE_H
#define CLIENTSPAGE_H

#pragma once

#include "../client/DCPlusPlus.h"

class ClientsPage : public EmptyPage
{
	public:
		ClientsPage(SettingsManager *s) : EmptyPage(s, TSTRING(SETTINGS_ADVANCED) + _T('\\') + TSTRING(SETTINGS_FAKEDETECT) + _T('\\') + TSTRING(SETTINGS_CLIENTS))
		{
		}
};

#endif //CLIENTSPAGE_H

/**
 * @file
 * $Id: ClientsPage.h 453 2009-08-04 15:46:31Z BigMuscle $
 */
