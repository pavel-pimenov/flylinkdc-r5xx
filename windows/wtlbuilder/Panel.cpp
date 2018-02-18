#include "stdafx.h"
#include "Panel.h"

namespace Panel
{
   /////////////////////////////////////////////////////////////////////////////
   CPanel::CPanel(void):caption(""),textcolor(COLOR_BTNTEXT),bkcolor(COLOR_BTNFACE),
      inner(BDR_RAISEDINNER),outer(BDR_RAISEDOUTER),edge(BF_RECT),vertAlign(DT_VCENTER),horizAlign(DT_CENTER),
      singleLine(DT_SINGLELINE)
   {
      font.CreateFont(-12,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
         PROOF_QUALITY,VARIABLE_PITCH | FF_ROMAN,"MS Sans Serif");
   }
   
   CPanel::~CPanel()
   {
   }
   // CBevelLine message handlers
   
   LRESULT CPanel::OnPaint(UINT, WPARAM,LPARAM,BOOL&)
   {
      CPaintDC dc(m_hWnd); // device context for painting
      CRect r;
      GetClientRect(&r);
      dc.FillSolidRect(&r,bkcolor);
      
      if(!caption.IsEmpty())
      {
         CFont oldFont=dc.SelectFont(font);
         COLORREF tc=dc.SetTextColor(textcolor);
         int bkm=dc.SetBkMode(TRANSPARENT);
         dc.DrawText(caption,caption.GetLength(),r,vertAlign|horizAlign|singleLine);
         dc.SetTextColor(tc);
         dc.SetBkMode(bkm);
         dc.SelectFont(oldFont);
      }
      
      dc.DrawEdge(&r,inner|outer,edge);
      return 0;
   }
   
   LRESULT CPanel::OnSetFont(UINT, WPARAM wParam,LPARAM lParam,BOOL&)
   {
      if(wParam)
      {
         LOGFONT lf;
         if(::GetObject((HFONT)wParam,sizeof(LOGFONT),&lf)!=0)
         {
            font.DeleteObject();
            font.CreateFontIndirect(&lf);
            if(lParam)
               InvalidateRect(NULL);
         }
      }
      return 0;
   }
   
   LRESULT CPanel::OnGetFont(UINT, WPARAM,LPARAM,BOOL& Handled)
   {
      Handled=TRUE;
      return (LRESULT)(HFONT)font;
   }
   
   LRESULT CPanel::OnEraseBkgnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
   {
      return TRUE;
   }
   
   void CPanel::SetInnerBorder(long val)
   {
      //if(inner!=val)
      {
         inner=val;
         Invalidate();
         UpdateWindow();
      }
   }
   
   long CPanel::GetInnerBorder(void)
   {
      return inner;
   }
   
   void CPanel::SetOuterBorder(long val)
   {
      //if(outer!=val)
      {
         outer=val;
         Invalidate();
         UpdateWindow();
      }
   }
   
   long CPanel::GetOuterBorder(void)
   {
      return outer;
   }
   
   void CPanel::SetEdgeType(long val)
   {
      //if(edge!=val)
      {
         edge=val;
         Invalidate();
         UpdateWindow();
      }
   }
   
   long CPanel::GetEdgeType(void)
   {
      return edge;
   }
   
   void CPanel::SetCaption(CString str)
   {
      //if(caption!=str)
      {
         caption=str;
         Invalidate();
      }
   }
   
   CString CPanel::GetCaption(void)
   {
      return caption;
   }
   
   void CPanel::SetTextColor(CColorref c)
   {
      //if(textcolor!=c)
      {
         textcolor=c;
         Invalidate();
      }
   }
   
   CColorref CPanel::GetTextColor(void)
   {
      return textcolor;
   }
   
   void CPanel::SetBkColor(CColorref c)
   {
      //if(bkcolor!=c)
      {
         bkcolor=c;
         Invalidate();
      }
   }
   
   CColorref CPanel::GetBkColor(void)
   {
      return bkcolor;
   }
   
   void CPanel::SetHorizTextAlign(long val)
   {
      horizAlign&=~(DT_LEFT|DT_CENTER|DT_RIGHT);
      horizAlign|=val;
      InvalidateRect(NULL);
   }
   
   long CPanel::GetHorizTextAlign(void)
   {
      return horizAlign & (DT_LEFT|DT_CENTER|DT_RIGHT);
   }
   
   void CPanel::SetVertTextAlign(long val)
   {
      vertAlign&=~(DT_TOP|DT_VCENTER|DT_BOTTOM);
      vertAlign|=val;
      InvalidateRect(NULL);
   }
   
   long CPanel::GetVertTextAlign(void)
   {
      return vertAlign & (DT_TOP|DT_VCENTER|DT_BOTTOM);
   }
   
   void CPanel::SetSingleLine(BOOL val)
   {
      if(val==TRUE)
         singleLine|=DT_SINGLELINE;
      else
         singleLine&=~DT_SINGLELINE;
      InvalidateRect(NULL);
   }
   
   BOOL CPanel::GetSingleLine(void)
   {
      return (singleLine & DT_SINGLELINE)!=0;
   }
   //////////////////////////////////////////////////////////////////////////
   CPanelHost::CPanelHost():current(-1)
   {
   }
   
   CPanelHost::~CPanelHost()
   {
   }
   
   void CPanelHost::AddPanel(CPanel * panel)
   {
      CRect rc;
      GetClientRect(&rc);
      panel->MoveWindow(rc);
      
      if(panels.Find(panel)==-1)
         panels.Add(panel);
      if(panels.GetSize()==1)
         SetCurrent(0l);
   }
   
   long CPanelHost::GetCurrent()
   {
      return current;
   }
   
   void CPanelHost::SetCurrent(long c)
   {
      if(c!=-1 && c < panels.GetSize())
      {
         if(current!=-1)
            panels[current]->ShowWindow(SW_HIDE);
         panels[c]->ShowWindow(SW_SHOW);
      }
      current=c;
   }
   
   void CPanelHost::SetCurrent(CPanel * panel)
   {
      int idx;
      if((idx=panels.Find(panel))!=-1)
         SetCurrent(idx);
   }
   
   LRESULT CPanelHost::OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
   {
      CRect rc;
      GetClientRect(&rc);
      for(int i=0; i < panels.GetSize(); i++)
         panels[i]->MoveWindow(rc);
      return 0;
   }
   
   LRESULT CPanelHost::OnShow(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
   {
      if(current!=-1 && wParam==TRUE)
         panels[current]->ShowWindow(SW_HIDE);
      return 0;
   }
   
   long CPanelHost::GetCount()
   {
      return panels.GetSize();
   }
   
   CPanel*CPanelHost::GetAt(long idx)
   {
      if(idx <panels.GetSize())
      {
         return panels[idx];
      }
      
      return NULL;
   }
};

