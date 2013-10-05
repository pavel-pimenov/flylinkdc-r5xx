#pragma once

#ifdef RIP_USE_SKIN

#ifndef GetClassLongPtr
#define GetClassLongPtr GetClassLong
#endif

#ifndef GetWindowLongPtr
#define GetWindowLongPtr GetWindowLong
#endif

#ifndef SetWindowLongPtr
#define SetWindowLongPtr SetWindowLong
#endif

class CWndWrapper
{
	public:
		CWndWrapper(HWND hWnd):
			m_hWnd(hWnd)
		{
		}
		
		CWndWrapper():
			m_hWnd(NULL)
		{
		};
		
		bool CreateWnd(HINSTANCE hInstance, LPCTSTR pszClassName, LPCTSTR pszName, int x, int y, int w, int h, WNDPROC pWndProc, HWND hParentWnd = NULL, LPARAM lParam = 0, DWORD dwExtraMem = 0, DWORD dwStyle = WS_CLIPCHILDREN | WS_OVERLAPPED | WS_VISIBLE, DWORD dwStyleEx = 0, HMENU hMenu = NULL, HICON hIcon = NULL)
		{
			_ASSERTE(m_hWnd == NULL);
			_ASSERTE(dwExtraMem > 0 || lParam == 0);
			
			if (!RegisterClass(hInstance, pszClassName, pWndProc, dwExtraMem * sizeof(LPARAM), hIcon) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) //-V104 //-V107
				return false;
				
			m_hWnd = CreateWindowEx
			         (
			             dwStyleEx,             // extended window style
			             pszClassName,          // pointer to registered class name
			             pszName,               // pointer to window name
			             dwStyle,               // window style
			             x, y,                  // horizontal and vertical position of window
			             w, h,                  // window width and height
			             hParentWnd,            // handle to parent or owner window
			             hMenu,                 // handle to menu, or child-window identifier
			             hInstance,             // handle to application instance
			             (LPVOID)lParam         // pointer to window-creation data
			         );
			         
			if (m_hWnd == INVALID_HANDLE_VALUE || m_hWnd == NULL)
			{
				UnregisterClass(pszClassName, hInstance);
				return false;
			}
			
			if (lParam)
				SetWindowLongPtr(m_hWnd, 0, (LONG_PTR)lParam);
				
			return true;
		}
		
		bool CreateDlg(HINSTANCE hInstance, LPCTSTR pszClassName, int iTemplateID, DLGPROC pWndProc, LPARAM lParam = 0, DWORD dwExtraMem = 0, HICON hIcon = NULL)
		{
			_ASSERTE(m_hWnd == NULL);
			_ASSERTE(dwExtraMem > 0 || lParam == 0);
			
			if (!RegisterClass(hInstance, pszClassName, (WNDPROC)pWndProc, dwExtraMem * sizeof(LPARAM) + DLGWINDOWEXTRA, hIcon)) //-V104 //-V107
				return false;
				
			m_hWnd = CreateDialogParam(hInstance, MAKEINTRESOURCE(iTemplateID), NULL, NULL, lParam);
			
			if (m_hWnd == INVALID_HANDLE_VALUE || m_hWnd == NULL)
			{
				UnregisterClass(pszClassName, hInstance);
				return false;
			}
			
			if (lParam > 0)
				SetWindowLongPtr(m_hWnd, 0, (LONG_PTR)lParam);
				
			return true;
		}
		
		void OnDestroy()
		{
			if (m_hWnd)
			{
				LPCTSTR pClass;
#ifndef WINCE
				LONG_PTR lAtom = GetClassLongPtr(m_hWnd, GCW_ATOM);
				pClass = (LPCTSTR)lAtom;
#else
				WCHAR wcClassName[256];
				GetClassName(m_hWnd, wcClassName, _countof(wcClassName));
				pClass = wcClassName;
#endif
				_ASSERTE(pClass);
				
#ifndef WINCE
				LONG_PTR lModule = GetClassLongPtr(m_hWnd, GCLP_HMODULE);
				_ASSERTE(lModule);
#else
				LONG_PTR lModule = nullptr;
#endif
				
				HWND hWnd = m_hWnd;
				m_hWnd = NULL;
				DestroyWindow(hWnd);
				
				BOOL bRet = UnregisterClass(pClass, (HINSTANCE)lModule);
				
				UNREFERENCED_PARAMETER(bRet || GetLastError() == ERROR_CLASS_HAS_WINDOWS);
				_ASSERTE(bRet);
			}
		}
		
		virtual ~CWndWrapper()
		{
			OnDestroy();
		}
		
		void *GetUserData(int ind = 0)
		{
			return (void*)(LONGLONG)GetWindowLongPtr(m_hWnd, ind * sizeof(LPARAM)); //-V104 //-V107
		}
		
		void SetUserData(void *pData, int ind = 0)
		{
#pragma warning(push)
#pragma warning(disable:4244)
			SetWindowLongPtr(m_hWnd, ind * sizeof(LPARAM), (LPARAM)pData); //-V104 //-V107
#pragma warning(pop)
		}
		
		void *GetUserDataDlg(int ind = 0)
		{
			return (void*)(LONGLONG)GetWindowLongPtr(m_hWnd, ind * sizeof(LPARAM) + DLGWINDOWEXTRA); //-V104 //-V107
		}
		
		operator HWND() const
		{
			return m_hWnd;
		}
		
		bool GetMessage()
		{
			bool bRet = false;
			MSG msg;
			
			if (::GetMessage(&msg, NULL, NULL, NULL))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
				bRet = true;
			}
			
			return bRet;
		}
		
		bool PeekMessage()
		{
			bool bRet = false;
			MSG msg;
			
			if (::PeekMessage(&msg, NULL, NULL, NULL, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
				bRet = true;
			}
			
			return bRet;
		}
		
		void Clear(HINSTANCE hInstance, LPCTSTR pszClassName)
		{
			if (m_hWnd)
			{
				DestroyWindow(m_hWnd);
				UnregisterClass(pszClassName, hInstance);
				
				m_hWnd = NULL;
			}
		}
		
		void Detach()
		{
			m_hWnd = NULL;
		}
	protected:
		HWND m_hWnd;
		
	private:
		bool RegisterClass(HINSTANCE hInstance, LPCTSTR pszClassName, WNDPROC pWndProc, int iClsExtra, HICON hIcon)
		{
			WNDCLASS wc =
			{
#ifndef WINCE
				CS_BYTEALIGNCLIENT | CS_PARENTDC,  //style
#else
				CS_PARENTDC,  //style
#endif
				pWndProc,                          //lpfnWndProc
				0,                                 //cbClsExtra
				iClsExtra,                         //cbWndExtra
				hInstance,                         //hInstance
				hIcon,                             //hIcon
				LoadCursor(NULL, IDC_ARROW),       //hCursor
#ifndef WINCE
				(HBRUSH)(COLOR_BTNFACE + 1),       //hbrBackground
#else
				(HBRUSH)(COLOR_WINDOW + 1),
#endif
				NULL,                              //lpszMenuName
				pszClassName                       //lpszClassName
			};
			
			if (!::RegisterClass(&wc))
				return false;
				
			return true;
		}
};

#endif // RIP_USE_SKIN