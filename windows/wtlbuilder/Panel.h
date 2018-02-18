#if !defined(AFX_CPANEL_H)
#define AFX_CPANEL_H

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "GDIUtil.h"
#include <atlcrack.h>

namespace Panel
{
   class CPanel : public CWindowImpl<CPanel>
   {
   protected:
      long		inner;
      long		outer;
      long		edge;
      CString		caption;
      CColorref	textcolor;
      CColorref	bkcolor;
      CFont		font;
      long		vertAlign;
      long		horizAlign;
      long		singleLine;
   public:
      DECLARE_WND_CLASS(NULL)
         
         CPanel(void);
      virtual ~CPanel();
      
      void SetInnerBorder(long);
      long GetInnerBorder(void);
      __declspec(property(get=GetInnerBorder, put=SetInnerBorder)) long InnerBorder;
      
      void SetOuterBorder(long);
      long GetOuterBorder(void);
      __declspec(property(get=GetOuterBorder, put=SetOuterBorder)) long OuterBorder;
      
      void SetEdgeType(long);
      long GetEdgeType(void);
      __declspec(property(get=GetEdgeType, put=SetEdgeType)) long EdgeType;
      
      void SetCaption(CString);
      CString GetCaption(void);
      __declspec(property(get=GetCaption, put=SetCaption)) CString Caption;
      
      void SetTextColor(CColorref);
      CColorref GetTextColor(void);
      __declspec(property(get=GetTextColor, put=SetTextColor)) CColorref TextColor;
      
      void SetBkColor(CColorref);
      CColorref GetBkColor(void);
      __declspec(property(get=GetBkColor, put=SetBkColor)) CColorref BkColor;
      
      void SetHorizTextAlign(long);
      long GetHorizTextAlign(void);
      __declspec(property(get=GetHorizTextAlign, put=SetHorizTextAlign)) long HorizTextAlign;
      
      void SetVertTextAlign(long);
      long GetVertTextAlign(void);
      __declspec(property(get=GetVertTextAlign, put=SetVertTextAlign)) long VertTextAlign;
      
      void SetSingleLine(BOOL);
      BOOL GetSingleLine(void);
      __declspec(property(get=GetSingleLine, put=SetSingleLine)) BOOL SingleLine;
      
      BEGIN_MSG_MAP(CPanel)
         MESSAGE_HANDLER(WM_PAINT, OnPaint)
         MESSAGE_HANDLER(WM_SETFONT,OnSetFont)
         MESSAGE_HANDLER(WM_GETFONT,OnGetFont)
         MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
         FORWARD_NOTIFICATIONS()
      END_MSG_MAP()
         
      LRESULT OnPaint(UINT, WPARAM,LPARAM,BOOL&);
      LRESULT OnSetFont(UINT, WPARAM,LPARAM,BOOL&);
      LRESULT OnGetFont(UINT, WPARAM,LPARAM,BOOL&);
      LRESULT OnEraseBkgnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
   };
   //////////////////////////////////////////////////////////////////////////
   typedef CSimpleArray<CPanel*> CPanels;
   
   class CPanelHost : public CPanel
   {
   protected:
      CPanels panels;
      long current;
   public:
      DECLARE_WND_CLASS(NULL)
         
         CPanelHost(void);
      virtual ~CPanelHost();
      
      void AddPanel(CPanel *);
      long GetCurrent();
      void SetCurrent(long);
      void SetCurrent(CPanel *);
      long GetCount();
      CPanel*GetAt(long);
      
      BEGIN_MSG_MAP(CPanelHost)
         MESSAGE_HANDLER(WM_SIZE, OnSize)
         MESSAGE_HANDLER(WM_SHOWWINDOW,OnShow) 
         FORWARD_NOTIFICATIONS()
         CHAIN_MSG_MAP(CPanel)
         END_MSG_MAP()
         LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
      LRESULT OnShow(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
   };
};
//////////////////////////////////////////////////////////////////////////
#endif 

