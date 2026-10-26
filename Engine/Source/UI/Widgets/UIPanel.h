#pragma once

#include "UI/UIWidget.h"

namespace Opaax
{
    /**
     * A container: a rect for children, draws nothing. The canvas root is one.
     * Not hit-testable by default (an empty rect must not take clicks).
     */
    class UIPanel final : public UIWidget
    {
        OPAAX_OBJECT(UIPanel, UIWidget)

    public:
        UIPanel() { bHitTestable = false; }
    };
}
