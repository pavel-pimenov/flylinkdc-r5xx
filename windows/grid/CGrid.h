/**
 * Based on "PropertyGrid control" - Bjarke Viksoe
 * 
 * wrapped by reg < entry.reg@gmail.com >
 */

#ifndef PROPERTY_GRID_WRAPPER__H
#define PROPERTY_GRID_WRAPPER__H

#include "CGridItem.h"

namespace net { namespace r_eg { namespace ui {

/**
 * Customization event actions for grid components
 */
class CGrid : public CPropertyGridCtrl
{
public:

    BEGIN_MSG_MAP(CPropertyGridImpl)
        MESSAGE_HANDLER(WM_LBUTTONDOWN, onLButtonClick)
        MESSAGE_HANDLER(WM_LBUTTONDBLCLK, onLButtonClick)
        CHAIN_MSG_MAP(CPropertyGridCtrl)
    END_MSG_MAP()

    /**
     * handler a left mouse button for grid components
     * events:
     * - default:   [item1] -> clicked item2 -> [activate item2] -> [handling a "OnKillFocus" on item1]
     * - change to: [item1] -> clicked item2 -> [handling a "OnKillFocus" on item1] -> [activate item2]
     */
    LRESULT onLButtonClick(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
    {
        // before invalidate indexes
        ::SetFocus(GetParent());
        return OnLButtonDown(uMsg, wParam, lParam, bHandled);
    }
};

}}}

#endif // PROPERTY_GRID_WRAPPER__H