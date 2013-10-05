#pragma once

#ifdef RIP_USE_SKIN

#include "Wnd.h"
#include "Bitmap.h"
#include <vector>

class CSkinableCmdBar
{
	protected:
		struct ITEM_STRUCT
		{
			ITEM_STRUCT(BYTE _bStyle, BYTE _bState, int _iImage, LPARAM _iCmd, LPARAM _lParam):
				bState(_bState),
				iImage(_iImage),
				iCmd(_iCmd),
				bStyle(_bStyle),
				lParam(_lParam)
			{}
			
			BYTE bState;
			BYTE bStyle;
			int iImage;
			LPARAM iCmd;
			LPARAM lParam;
		};
		
	private:
		CWndWrapper m_Wnd;
		WORD m_wItemW, m_wItemH, m_wArrowLen, m_wSkinItemCount;
		DWORD m_dwSkinLen, m_dwWndLen;
		bool m_bUseArrows;
		
		COLORREF m_clrBack;
		
		DWORD m_dwHilitedItemInd;
		DWORD m_dwPushingItemInd;
		BYTE m_bHilitedArrowInd;
		BYTE m_bPushingArrowInd;
		
		int m_iOffset;
		BYTE m_bOverlap;
		bool m_bLeftOnTop;
		
		CBmp m_WorkImg;
		CBmp m_NormalImg;
		CBmp m_HilitedImg;
		CBmp m_SelectedImg;
		CBmp m_SelectedHilitedImg;
		CBmp m_TemplateImg;
		CBmp m_ArrowsImg;
		
		HIMAGELIST m_hNormalImgList;
		HIMAGELIST m_hHilightImgList;
		HIMAGELIST m_hSelectedImgList;
		HIMAGELIST m_hSelectedHilitedImgList;
		
		typedef std::vector<ITEM_STRUCT> tSTATE;
		tSTATE m_State;
		
		static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
		
		bool InitializeSkin(size_t szItemCount,
		                    HBITMAP hNormalBMP, HBITMAP hHilitedBMP, HBITMAP hSelectedBMP, HBITMAP hSelectedHilitedBMP, HBITMAP hArrowsBMP,
		                    COLORREF clrBack);
		void ClearHilited();
		void DrawArrows(bool bLeft, bool bRight, bool bHilited, bool bPushed, bool bDrawBackground);
		void DrawItem(DWORD dwItem, bool bHilited, bool bForceSelected, bool bInternalCall = false);
		
		void OnPaint(PAINTSTRUCT *pps);
		void OnMouseMove(WORD x, WORD y, WPARAM dwFlags);
		void OnMouseDown(WORD x, WORD y, WPARAM dwFlags);
		void OnMouseUp(WORD x, WORD y, WPARAM dwFlags);
		void OnSize(WORD w, WORD h, WPARAM dwFlags);
		void OnGetMinMaxInfo(MINMAXINFO *pMinMaxInfo);
		void OnTimer();
		
		DWORD GetVisibleLen();
		
		bool IsVert() const
		{
			return (m_Type & TYPE_VERTICAL) != 0;
		};
		template<typename T>
		void MapCoord(T &x, T &y) const;
		
		DWORD GetItemPromPos(int iPos);
		
		int AddItemToSkin(HICON hIcon, CBmp &Img, DWORD dwItem, BYTE bState);
		
	protected:
		virtual void OnCommand(DWORD dwItem, ITEM_STRUCT &is);
		
	public:
		CSkinableCmdBar();
		~CSkinableCmdBar();
		
		operator HWND()
		{
			return m_Wnd;
		}
		
		enum DefinedTypeFlags
		{
			TYPE_HORIZONTAL = 0x0000,
			TYPE_VERTICAL   = 0x0001,
		};
		
		bool Create(HWND hParentWnd, int x, int y, int len, DefinedTypeFlags Type,  size_t szItemCount,
		            HBITMAP hNormalBMP, HBITMAP hHilitedBMP, HBITMAP hSelectedBMP, HBITMAP hSelectedHilitedBMP, HBITMAP hArrowsBMP,
		            COLORREF clrBack, int iOverlap = 0);
		            
		bool Create(HWND hParentWnd, int x, int y, int len, DefinedTypeFlags Type, HBITMAP hArrowsBMP, HBITMAP hTemplate, COLORREF clrBack, int iOverlap = 0);
		
		DWORD GetItemByCmd(LPARAM iCmd);
		DWORD AddItem(int iImage, LPARAM iCmd, BYTE bStyle = BS_CHECKBOX, BYTE bState = BST_UNCHECKED, LPARAM lParam = 0, bool bUpdateAll = false);
		DWORD AddItem(HICON hIcon, LPARAM iCmd, BYTE bStyle = BS_CHECKBOX, BYTE bState = BST_UNCHECKED, LPARAM lParam = 0);
		bool RemoveItem(DWORD dwItem);
		void SetItemState(DWORD dwItem, BYTE bState, bool bUpdate = false);
		
		void Update();
		DWORD GetHeight() const;
		
	private:
		DefinedTypeFlags m_Type: 8;
};

#endif