
#include "stdafx.h"


#ifdef IRAINMAN_INCLUDE_OLD_CLIENT_PROFILE_MANAGER

#include "Resource.h"
#include "WinUtil.h"
#include "ClientProfileDlg.h"

LRESULT ClientProfileDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	if (currentProfileId != -1)
	{
		// FIXME disable this for now to stop potential dupes (ahh. fuck it, leave it enabled :p)
		//::EnableWindow(GetDlgItem(IDC_CLIENT_NAME), false);
		adding = false;
		getProfile();
	}
	else
	{
		adding = true;
		::EnableWindow(GetDlgItem(IDC_NEXT), false);
	}
	
	ATTACH(IDC_CLIENT_NAME, ctrlName);
	ATTACH(IDC_CLIENT_VERSION, ctrlVersion);
	ATTACH(IDC_CLIENT_TAG, ctrlTag);
	ATTACH(IDC_CLIENT_EXTENDED_TAG, ctrlExtendedTag);
#ifdef IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY
	ATTACH(IDC_CLIENT_LOCK, ctrlLock);
	ATTACH(IDC_CLIENT_PK, ctrlPk);
#endif
	ATTACH(IDC_CLIENT_SUPPORTS, ctrlSupports);
	//ATTACH(IDC_CLIENT_TESTSUR_RESPONSE, ctrlTestSUR);
	ATTACH(IDC_CLIENT_USER_CON_COM, ctrlUserConCom);
	ATTACH(IDC_CLIENT_STATUS, ctrlStatus);
	ATTACH(IDC_CLIENT_CHEATING_DESCRIPTION, ctrlCheatingDescription);
	ATTACH(IDC_CLIENT_CONNECTION, ctrlConnection);
	
	ATTACH(IDC_USE_EXTRA_VERSION, ctrlUseExtraVersion);
	ATTACH(IDC_VERSION_MISMATCH, ctrlCheckMismatch);
	
	ATTACH(IDC_ADD_LINE, ctrlAddLine);
	ATTACH(IDC_COMMENT, ctrlComment);
	
	ATTACH(IDC_CLIENT_PROFILE_RAW, ctrlRaw);
	ctrlRaw.AddString(CTSTRING(NO_ACTION));
	ctrlRaw.AddString(Text::toT(SETTING(RAW1_TEXT)).c_str());
	ctrlRaw.AddString(Text::toT(SETTING(RAW2_TEXT)).c_str());
	ctrlRaw.AddString(Text::toT(SETTING(RAW3_TEXT)).c_str());
	ctrlRaw.AddString(Text::toT(SETTING(RAW4_TEXT)).c_str());
	ctrlRaw.AddString(Text::toT(SETTING(RAW5_TEXT)).c_str());
	
	ATTACH(IDC_REGEXP_TESTER_COMBO, ctrlRegExpCombo);
	ctrlRegExpCombo.AddString(_T("Version"));
	ctrlRegExpCombo.AddString(_T("Tag"));
	ctrlRegExpCombo.AddString(_T("Description"));
#ifdef IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY
	ctrlRegExpCombo.AddString(_T("Lock"));
	ctrlRegExpCombo.AddString(_T("Pk"));
#endif
	ctrlRegExpCombo.AddString(_T("Supports"));
	//ctrlRegExpCombo.AddString(_T("TestSUR"));
	ctrlRegExpCombo.AddString(_T("Commands"));
	ctrlRegExpCombo.AddString(_T("Status"));
	ctrlRegExpCombo.AddString(_T("Connection"));
	ctrlRegExpCombo.SetCurSel(0);
#ifdef IRAINMAN_INCLUDE_OLD_CLIENT_PROFILE_MANAGER
	params = ClientProfileManager::getInstance()->getParams();
#else
	params = DetectionManager::getInstance()->getParams();
#endif
	
	updateControls();
	
	CenterWindow(GetParent());
	return FALSE;
}

LRESULT ClientProfileDlg::onChange(WORD , WORD , HWND , BOOL&)
{
	updateAddLine();
	return 0;
}

LRESULT ClientProfileDlg::onChangeTag(WORD , WORD , HWND , BOOL&)
{
	updateAddLine();
	updateTag();
	return 0;
}

void ClientProfileDlg::updateTag()
{
	tstring exp;
	GET_TEXT(IDC_CLIENT_TAG, exp);
	exp = Text::toT(Util::formatRegExp(Text::fromT(exp), params));
	SetDlgItemText(IDC_CLIENT_FORMATTED_TAG, exp.c_str());
}
void ClientProfileDlg::updateAddLine()
{
	addLine.clear();
	LocalArray<TCHAR, BUF_LEN> buf;
	
#define UPDATE \
	GetWindowText(buf, BUF_LEN-1); \
	addLine += buf; \
	addLine += _T(';');
	
	ctrlName.UPDATE;
	ctrlVersion.UPDATE;
	ctrlTag.UPDATE;
	ctrlExtendedTag.UPDATE;
	//ctrlLock.UPDATE; // IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY
	//ctrlPk.UPDATE; // IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY
	ctrlSupports.UPDATE;
	//ctrlTestSUR.UPDATE;
	ctrlUserConCom.UPDATE;
	ctrlStatus.UPDATE;
	ctrlConnection.UPDATE;
	ctrlCheatingDescription.GetWindowText(buf, BUF_LEN - 1);
	addLine += buf;
	
	ctrlAddLine.SetWindowText(addLine.c_str());
}

void ClientProfileDlg::getProfile()
{
#ifdef IRAINMAN_INCLUDE_OLD_CLIENT_PROFILE_MANAGER
	ClientProfileManager::getInstance()->getClientProfile(currentProfileId, currentProfile);
#else
	DetectionManager::getInstance()->getProfiles(currentProfile);
#endif
	
	name = Text::toT(currentProfile.getName());
	version = Text::toT(currentProfile.getVersion());
	tag = Text::toT(currentProfile.getTag());
	extendedTag = Text::toT(currentProfile.getExtendedTag());
#ifdef IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY
	lock = Text::toT(currentProfile.getLock());
	pk = Text::toT(currentProfile.getPk());
#endif
	supports = Text::toT(currentProfile.getSupports());
	//testSUR = Text::toT(currentProfile.getTestSUR());
	userConCom = Text::toT(currentProfile.getUserConCom());
	status = Text::toT(currentProfile.getStatus());
	cheatingDescription = Text::toT(currentProfile.getCheatingDescription());
	rawToSend = currentProfile.getRawToSend();
//  tagVersion = currentProfile.getTagVersion();
	useExtraVersion = currentProfile.getUseExtraVersion() == 1;
	checkMismatch = currentProfile.getCheckMismatch() == 1;
	connection = Text::toT(currentProfile.getConnection());
	comment = Text::toT(currentProfile.getComment());
}

void ClientProfileDlg::updateVars()
{
	GET_TEXT(IDC_CLIENT_NAME, name);
	GET_TEXT(IDC_CLIENT_VERSION, version);
	GET_TEXT(IDC_CLIENT_TAG, tag);
	GET_TEXT(IDC_CLIENT_EXTENDED_TAG, extendedTag);
	GET_TEXT(IDC_CLIENT_LOCK, lock);
	GET_TEXT(IDC_CLIENT_PK, pk);
	GET_TEXT(IDC_CLIENT_SUPPORTS, supports);
	//GET_TEXT(IDC_CLIENT_TESTSUR_RESPONSE, testSUR);
	GET_TEXT(IDC_CLIENT_USER_CON_COM, userConCom);
	GET_TEXT(IDC_CLIENT_STATUS, status);
	GET_TEXT(IDC_CLIENT_CHEATING_DESCRIPTION, cheatingDescription);
	GET_TEXT(IDC_CLIENT_CONNECTION, connection);
	GET_TEXT(IDC_COMMENT, comment);
	//tagVersion = 0;//(ctrlTagVersion.GetCheck() == BST_CHECKED) ? 1 : 0;
	useExtraVersion = ctrlUseExtraVersion.GetCheck() == BST_CHECKED;
	checkMismatch = ctrlCheckMismatch.GetCheck() == BST_CHECKED;
	
	rawToSend = ctrlRaw.GetCurSel();
}

void ClientProfileDlg::updateControls()
{
	ctrlName.SetWindowText(name.c_str());
	ctrlVersion.SetWindowText(version.c_str());
	ctrlTag.SetWindowText(tag.c_str());
	ctrlExtendedTag.SetWindowText(extendedTag.c_str());
#ifdef IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY
	ctrlLock.SetWindowText(lock.c_str());
	ctrlPk.SetWindowText(pk.c_str());
#endif
	ctrlSupports.SetWindowText(supports.c_str());
	//ctrlTestSUR.SetWindowText(testSUR.c_str());
	ctrlUserConCom.SetWindowText(userConCom.c_str());
	ctrlStatus.SetWindowText(status.c_str());
	ctrlCheatingDescription.SetWindowText(cheatingDescription.c_str());
	ctrlAddLine.SetWindowText(addLine.c_str());
	ctrlConnection.SetWindowText(connection.c_str());
	ctrlComment.SetWindowText(comment.c_str());
	
	//ctrlTagVersion.SetCheck((tagVersion) ? BST_CHECKED : BST_UNCHECKED);
	ctrlUseExtraVersion.SetCheck(useExtraVersion ? BST_CHECKED : BST_UNCHECKED);
	ctrlCheckMismatch.SetCheck(checkMismatch ? BST_CHECKED : BST_UNCHECKED);
	
	ctrlRaw.SetCurSel(rawToSend);
	
	ctrlName.SetFocus();
}

LRESULT ClientProfileDlg::onNext(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{

	updateVars();
	
	currentProfile.setName(Text::fromT(name));
	currentProfile.setVersion(Text::fromT(version));
	currentProfile.setTag(Text::fromT(tag));
	currentProfile.setExtendedTag(Text::fromT(extendedTag));
#ifdef IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY
	currentProfile.setLock(Text::fromT(lock));
	currentProfile.setPk(Text::fromT(pk));
#endif
	currentProfile.setSupports(Text::fromT(supports));
	//currentProfile.setTestSUR(Text::fromT(testSUR));
	currentProfile.setUserConCom(Text::fromT(userConCom));
	currentProfile.setStatus(Text::fromT(status));
	currentProfile.setCheatingDescription(Text::fromT(cheatingDescription));
	currentProfile.setRawToSend(rawToSend);
//  currentProfile.setTagVersion(tagVersion);
	currentProfile.setUseExtraVersion(useExtraVersion);
	currentProfile.setCheckMismatch(checkMismatch);
	currentProfile.setConnection(Text::fromT(connection));
	currentProfile.setComment(Text::fromT(comment));
#ifdef IRAINMAN_INCLUDE_OLD_CLIENT_PROFILE_MANAGER
	ClientProfileManager::getInstance()->updateClientProfile(currentProfile);
#else
	DetectionManager::getInstance()->updateClientProfile(currentProfile);
#endif
	
	currentProfileId++;
	getProfile();
	updateControls();
	
	return 0;
}

LRESULT ClientProfileDlg::onMatch(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring exp, text;
	GET_TEXT(IDC_REGEXP_TESTER_TEXT, text);
	switch (ctrlRegExpCombo.GetCurSel())
	{
		case 0:
			GET_TEXT(IDC_CLIENT_VERSION, exp);
			break;
		case 1:
		{
			tstring version;
			tstring versionExp;
			GET_TEXT(IDC_CLIENT_TAG, exp);
			tstring formattedExp = exp;
			/* [-]PPA
			            tstring::size_type j = exp.find(_T("%[version]"));
			            if (j != string::npos)
			            {
			                formattedExp.replace(j, 10, _T(".*"));
			                version = Text::toT(getVersion(Text::fromT(exp), Text::fromT(text)));
			                GET_TEXT(IDC_CLIENT_VERSION, versionExp)
			            }
			*/
			switch (matchExp(Text::fromT(formattedExp), Text::fromT(text)))
			{
				case MATCH:
					break;
				case MISMATCH:
					MessageBox("No match for tag.", "RegExp Tester", MB_OK);
					return 0;
				case INVALID:
					MessageBox("Invalid tag RegExp.", "RegExp Tester", MB_OK);
					return 0;
			}
			if (version.empty())
			{
				MessageBox("It's a match!", "RegExp Tester", MB_OK);
				return 0;
			}
			else
			{
				switch (matchExp(Text::fromT(versionExp), Text::fromT(version)))
				{
					case MATCH:
						MessageBox("It's a match!", "RegExp Tester", MB_OK);
						return 0;
					case MISMATCH:
						MessageBox("No match for version.", "RegExp Tester", MB_OK);
						return 0;
					case INVALID:
						MessageBox("Invalid version RegExp.", "RegExp Tester", MB_OK);
						return 0;
				}
			}
		}
		case 2:
		{
			GET_TEXT(IDC_CLIENT_EXTENDED_TAG, exp);
			string::size_type j = exp.find(_T("%[version2]"));
			if (j != string::npos)
			{
				exp.replace(j, 11, _T(".*"));
			}
			break;
		}
		case 3: // TODO: cut this without has been defined IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY
			GET_TEXT(IDC_CLIENT_LOCK, exp);
			break;
		case 4: // TODO: cut this without has been defined IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY
		{
			tstring version;
			tstring versionExp;
			GET_TEXT(IDC_CLIENT_PK, exp);
			tstring formattedExp = exp;
			/* [-]PPA
			            tstring::size_type j = exp.find(_T("%[version]"));
			            if (j != string::npos)
			            {
			                formattedExp.replace(j, 10, _T(".*"));
			                version = Text::toT(getVersion(Text::fromT(exp), Text::fromT(text)));
			                GET_TEXT(IDC_CLIENT_VERSION, versionExp, 1024)
			            }
			*/
			switch (matchExp(Text::fromT(formattedExp), Text::fromT(text)))
			{
				case MATCH:
					break;
				case MISMATCH:
					MessageBox("No match for Pk.", "RegExp Tester", MB_OK);
					return 0;
				case INVALID:
					MessageBox("Invalid Pk RegExp.", "RegExp Tester", MB_OK);
					return 0;
			}
			if (version.empty())
			{
				MessageBox("It's a match!", "RegExp Tester", MB_OK);
				return 0;
			}
			else
			{
				switch (matchExp(Text::fromT(versionExp), Text::fromT(version)))
				{
					case MATCH:
						MessageBox("It's a match!", "RegExp Tester", MB_OK);
						return 0;
					case MISMATCH:
						MessageBox("No match for version.", "RegExp Tester", MB_OK);
						return 0;
					case INVALID:
						MessageBox("Invalid version RegExp.", "RegExp Tester", MB_OK);
						return 0;
				}
			}
		}
		case 5:
			GET_TEXT(IDC_CLIENT_SUPPORTS, exp);
			break;
		case 6:
			GET_TEXT(IDC_CLIENT_TESTSUR_RESPONSE, exp);
			break;
		case 7:
			GET_TEXT(IDC_CLIENT_USER_CON_COM, exp);
			break;
		case 8:
			GET_TEXT(IDC_CLIENT_STATUS, exp);
			break;
		case 9:
			GET_TEXT(IDC_CLIENT_CONNECTION, exp);
			break;
		default:
			dcdebug("We shouldn't be here!\n");
	}
	switch (matchExp(Text::fromT(exp), Text::fromT(text)))
	{
		case MATCH:
			MessageBox("It's a match!", "RegExp Tester", MB_OK);
			break;
		case MISMATCH:
			MessageBox("No match.", "RegExp Tester", MB_OK);
			break;
		case INVALID:
			MessageBox("Invalid RegExp!", "RegExp Tester", MB_OK);
			break;
	}
	return 0;
}

int ClientProfileDlg::matchExp(const string& aExp, const string& aString)
{
	try
	{
		const regex reg(aExp);
		return regex_search(aString.begin(), aString.end(), reg) ? MATCH : MISMATCH;
	}
	catch (const regex_error& e)
	{
		//return _T("Invalid RegExp!") + _T(" Error: ") + Text::toT(e.what());
	}
	return INVALID;
}

#endif // IRAINMAN_INCLUDE_DETECTION_MANAGER

#ifdef IRAINMAN_INCLUDE_OLD_CLIENT_PROFILE_MANAGER

string ClientProfileDlg::getVersion(const string& aExp, const string& aTag)
{
	string::size_type i = aExp.find("%[version]");
	if (i == string::npos)
	{
		i = aExp.find("%[version2]");
		return splitVersion(aExp.substr(i + 11), splitVersion(aExp.substr(0, i), aTag, 1), 0);
	}
	return splitVersion(aExp.substr(i + 10), splitVersion(aExp.substr(0, i), aTag, 1), 0);
}

string ClientProfileDlg::splitVersion(const string& aExp, const string& aTag, const int part)
{
	PME reg(aExp);
	if (!reg.IsValid())
	{
		return "";
	}
	reg.split(aTag, 2);
	return reg[part];
}

#endif // IRAINMAN_INCLUDE_OLD_CLIENT_PROFILE_MANAGER