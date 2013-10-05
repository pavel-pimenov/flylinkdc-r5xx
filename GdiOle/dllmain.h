// dllmain.h : Declaration of module class.
#ifdef _USRDLL

class CGdiOleModule : public CAtlDllModuleT< CGdiOleModule >
{
	public :
		DECLARE_LIBID(LIBID_GdiOleLib)
		DECLARE_REGISTRY_APPID_RESOURCEID(IDR_GdiOle, "{68A116EC-E218-48FE-9E98-9998DFFD2921}")
};

extern class CGdiOleModule _AtlModule;
#endif