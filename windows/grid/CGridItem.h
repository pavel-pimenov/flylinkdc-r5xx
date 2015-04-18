/**
 * Based on "PropertyGrid control" - Bjarke Viksoe
 * 
 * wrapped by reg < entry.reg@gmail.com >
 */

#ifndef PROPERTY_GRID_WRAPPER_ITEM__H
#define PROPERTY_GRID_WRAPPER_ITEM__H

#include "PropertyGrid.h"
#include <vector>

namespace net { namespace r_eg { namespace ui {

struct ICGridEventKeys
{
    virtual LRESULT onKeyHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) = 0;
};
typedef ICGridEventKeys* ICGridEventKeys_;

struct TCGridItemCustom
{
    LPCTSTR caption;
    RECT offset;
    UINT flags;

    TCGridItemCustom(): offset(){ memset(&offset, 0, sizeof(RECT)); };
};


/**
 * Custom item types for PropertyGrid
 */
class CGridItem
{
public:

    template<class Type>
    static HPROPERTY EditBox(Type val, LPCTSTR name, LPARAM lParam = 0)
    {
        return new CEditBox(CComVariant(val), name, lParam);
    }

    template<class Type>
    static HPROPERTY EditBox(Type val, LPARAM lParam = 0)
    {
        return new CEditBox(CComVariant(val), _T(""), lParam);
    }

    template<class Type>
    static HPROPERTY EditBox(Type val, ICGridEventKeys_ evt, LPCTSTR name, LPARAM lParam = 0)
    {
        return new CEditBox(CComVariant(val), evt, name, lParam);
    }

    template<class Type>
    static HPROPERTY EditBox(Type val, ICGridEventKeys_ evt, LPARAM lParam = 0)
    {
        return new CEditBox(CComVariant(val), evt, _T(""), lParam);
    }

    static HPROPERTY CheckBox(bool val, LPCTSTR name = _T(""), LPARAM lParam = 0)
    {
        return new CCheckBox(val, name, lParam);
    }

    static HPROPERTY ButtonPush(bool val, LPCTSTR caption, LPCTSTR name = _T(""), LPARAM lParam = 0)
    {
        return new CButtonPush(val, caption, name, lParam);
    }

    static HPROPERTY ButtonPush(bool val, TCGridItemCustom custom, LPCTSTR name = _T(""), LPARAM lParam = 0)
    {
        return new CButtonPush(val, custom, name, lParam);
    }

    static HPROPERTY Button(LPCTSTR caption, LPCTSTR name = _T(""), LPARAM lParam = 0)
    {
        return new CButton(caption, name, lParam);
    }

    static HPROPERTY Button(TCGridItemCustom custom, LPCTSTR name = _T(""), LPARAM lParam = 0)
    {
        return new CButton(custom, name, lParam);
    }

    static HPROPERTY ListBox(std::vector<LPCTSTR> list, TCGridItemCustom custom, int sel = 0, LPCTSTR name = _T(""), LPARAM lParam = 0)
    {
        list.push_back(NULL);
        LPCTSTR* strList = &list[0];
        return new CListBox(strList, custom, sel, name, lParam);
    }

    static HPROPERTY ListBox(LPCTSTR* strList, int sel = 0, LPCTSTR name = _T(""), LPARAM lParam = 0)
    {
        return new CListBox(strList, sel, name, lParam);
    };

protected:

    CGridItem(){};

    /** CEditBox */

    class CEditBox : public CPropertyEditItem, ICGridEventKeys
    {
    public:

        CEditBox(CComVariant val, LPCTSTR name, LPARAM lParam): 
          CPropertyEditItem(name, CComVariant(val), lParam),
          evt(this)
        {
            m_val = val;
        }

        CEditBox(CComVariant val, ICGridEventKeys_ evt, LPCTSTR name, LPARAM lParam): 
          CPropertyEditItem(name, CComVariant(val), lParam),
          evt(evt)
        {
            m_val = val;
        }

        LRESULT onKeyHandler(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
        {
            switch(uMsg){
                case WM_KEYDOWN:{
                    bHandled = FALSE;
                    return 0;
                }
                case WM_KEYUP:{
                    bHandled = FALSE;
                    return 0;
                }
                case WM_CHAR:{
                    bHandled = FALSE;
                    return 0;
                }
            }
            bHandled = FALSE;
            return 0;
        }

        BOOL GetValue(VARIANT* pVal) const
        {
            if(!::IsWindow(m_hwndEdit)){
                return SUCCEEDED( CComVariant(m_val).Detach(pVal) );
            }
            
            TCHAR title[256] = {0};
            ::GetWindowText(m_hwndEdit, title, sizeof(title) / sizeof(TCHAR));
            return SUCCEEDED( CComVariant(title).Detach(pVal) );
        }

        HWND CreateInplaceControl(HWND hWnd, const RECT& rc) 
        {
            UINT len    = GetDisplayValueLength() + 1;
            LPTSTR pstr = (LPTSTR)_alloca(len * sizeof(TCHAR));

            ATLASSERT(pstr);
            if(!GetDisplayValue(pstr, len)){
                return NULL;
            }

            RECT rect = rc;
            CEditBoxComponent* c = new CEditBoxComponent(evt);
            m_hwndEdit           = c->Create(hWnd, rect, pstr, WS_VISIBLE | WS_CHILD | ES_LEFT | ES_AUTOHSCROLL);

            ATLASSERT(::IsWindow(m_hwndEdit));
            return m_hwndEdit;
        }

        BOOL Activate(UINT action, LPARAM /*lParam*/)
        {
            if(::IsWindow(m_hwndEdit)){
                ::SetFocus(m_hwndEdit);
                //TODO:
                //::SendMessage(m_hwndEdit, EM_SETSEL, 0, -1);
            }
            return TRUE;
        }

    protected:
        ICGridEventKeys_ evt;
    };

    class CEditBoxComponent: public CPropertyEditWindow
    {
    public:

        CEditBoxComponent(ICGridEventKeys_ evt): 
            CPropertyEditWindow(),
            evt(evt)
        {
        }
       
        BEGIN_MSG_MAP(CEditBoxComponent)
            MESSAGE_HANDLER(WM_CHAR, onKeyHandler)
            MESSAGE_HANDLER(WM_KEYDOWN, onKeyHandler)
            MESSAGE_HANDLER(WM_KEYUP, onKeyHandler)
            CHAIN_MSG_MAP(CPropertyEditWindow)
        END_MSG_MAP()

        LRESULT onKeyHandler(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
        {
            return evt->onKeyHandler(uMsg, wParam, lParam, bHandled);
        };

    protected:
        ICGridEventKeys_ evt;
    };

    /** CCheckBox */

    class CCheckBox : public CPropertyCheckButtonItem
    {
    public:

        CCheckBox(bool val, LPCTSTR name, LPARAM lParam): 
            CPropertyCheckButtonItem(name, val, lParam)
        {
            
        }

        void DrawValue(PROPERTYDRAWINFO& di)
        {
            UINT state = DFCS_BUTTONCHECK;

            if(m_bValue){
                state |= DFCS_CHECKED;
            }

            if((di.state & ODS_DISABLED) != 0){
                state |= DFCS_INACTIVE;
            }
            ::DrawFrameControl(di.hDC, &di.rcItem, DFC_BUTTON, state);
        }
    };

    /** CButton */

    class CButton : public CPropertyCheckButtonItem
    {
    public:

        CButton(LPCTSTR text, LPCTSTR name, LPARAM lParam): 
            CPropertyCheckButtonItem(name, false, lParam)
        {
            custom.caption  = text;
            custom.flags    = DT_SINGLELINE | DT_VCENTER | DT_CENTER;
        }

        CButton(TCGridItemCustom custom, LPCTSTR name, LPARAM lParam): 
            CPropertyCheckButtonItem(name, false, lParam),
            custom(custom)
        {
            
        }

        void DrawValue(PROPERTYDRAWINFO& di)
        {
            basicImpl(DFCS_BUTTONPUSH, di);
        }

        BOOL GetDisplayValue(LPTSTR pstr, UINT len) const
        {
            ATLASSERT(!::IsBadStringPtr(pstr, len));
            ::lstrcpyn(pstr, custom.caption, len);
            return TRUE;
        }

        UINT GetDisplayValueLength() const
        {
            return _tcslen(custom.caption);
        }

    protected:
        TCGridItemCustom custom;

        void basicImpl(UINT state, PROPERTYDRAWINFO& di)
        {
            if((di.state & ODS_DISABLED) != 0){
                state |= DFCS_INACTIVE;
            }
            ::DrawFrameControl(di.hDC, &di.rcItem, DFC_BUTTON, state);

            RECT rc     = di.rcItem;
            rc.top      += custom.offset.top;
            rc.left     += custom.offset.left;
            rc.right    += custom.offset.right;
            rc.bottom   += custom.offset.bottom;

            ::DrawText(di.hDC, custom.caption, _tcslen(custom.caption), &rc, custom.flags);
        }

    };

    /** CButtonPush */

    class CButtonPush : public CButton
    {
    public:

        CButtonPush(bool val, LPCTSTR text, LPCTSTR name, LPARAM lParam): 
            CButton(text, name, lParam)
        {
            m_bValue = val;
        }

        CButtonPush(bool val, TCGridItemCustom custom, LPCTSTR name, LPARAM lParam): 
            CButton(custom, name, lParam)
        {
            m_bValue = val;
        }

        void DrawValue(PROPERTYDRAWINFO& di)
        {
            UINT state = DFCS_BUTTONPUSH;

            if(m_bValue){
                state |= DFCS_CHECKED;
            }

            basicImpl(state, di);
        }

    };

    /** CListBox */

    class CListBox : public CPropertyListItem
    {

    public:

        CListBox(LPCTSTR* strList, int sel, LPCTSTR name, LPARAM lParam) : 
            CPropertyListItem(name, strList, sel, lParam)
        {
            
        }

        CListBox(LPCTSTR* strList, TCGridItemCustom custom, int sel, LPCTSTR name, LPARAM lParam) : 
            CPropertyListItem(name, strList, sel, lParam),
            custom(custom)
        {
            
        }

        HWND CreateInplaceControl(HWND hWnd, const RECT& rect) 
        {
            UINT len    = GetDisplayValueLength() + 1;
            LPTSTR pstr = (LPTSTR)_alloca(len * sizeof(TCHAR));

            ATLASSERT(pstr);
            if(!GetDisplayValue(pstr, len)){
                return NULL;
            }
            
            RECT rc     = rect;
            rc.top      += custom.offset.top;
            rc.left     += custom.offset.left;
            rc.right    += custom.offset.right;
            rc.bottom   += custom.offset.bottom;

            CPropertyListWindow* c  = new CPropertyListWindow();
            m_hwndCombo             = c->Create(hWnd, rc, pstr);
            ATLASSERT(::IsWindow(m_hwndCombo));

            fill(m_arrList, m_val.lVal, c);
            return *c;
        }

        void DrawValue(PROPERTYDRAWINFO& di)
        {
            // Caption

            UINT len    = GetDisplayValueLength() + 1;
            LPTSTR pstr = (LPTSTR)_alloca(len * sizeof(TCHAR));

            ATLASSERT(pstr);
            if(!GetDisplayValue(pstr, len)){
                return;
            }
            
            CDCHandle dc(di.hDC);
            dc.SetBkMode(TRANSPARENT);
            dc.SetBkColor(di.clrBack);
            dc.SetTextColor(((di.state & ODS_DISABLED) != 0)? di.clrDisabled : di.clrText);

            dc.DrawText(pstr, -1, &di.rcItem, DT_LEFT | DT_SINGLELINE | DT_EDITCONTROL | DT_NOPREFIX | DT_END_ELLIPSIS | DT_VCENTER);


            // Button

            const int width = 14;
            RECT rc         = di.rcItem;
            UINT state      = DFCS_SCROLLDOWN;

            if((di.state & ODS_DISABLED) != 0){
                state |= DFCS_INACTIVE;
            }

            rc.left  = di.rcItem.right - width;
            rc.right = di.rcItem.right;

            ::DrawFrameControl(di.hDC, &rc, DFC_SCROLL, state);
        }

    protected:
        TCGridItemCustom custom;

        void fill(CSimpleArray<CComBSTR> list, LONG sel, CPropertyListWindow* c)
        {
            USES_CONVERSION;
            for(int i = 0; i < list.GetSize(); i++){
                c->AddItem(OLE2CT(list[i]));
            }
            c->SelectItem(sel);
        }
    };

};

}}};

#endif // PROPERTY_GRID_WRAPPER_ITEM__H