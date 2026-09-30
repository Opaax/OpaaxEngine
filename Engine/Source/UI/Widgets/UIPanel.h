#pragma once

#include "Core/String/OpaaxStringID.hpp"
#include "UI/UIWidget.h"

namespace Opaax
{
    /**
     * A container: a rect for children, draws nothing. The canvas root is one.
     * Not hit-testable by default (an empty rect must not take clicks).
     */
    class OPAAX_API UIPanel final : public UIWidget
    {
    public:
        UIPanel() { bHitTestable = false; }

        /**
         * Empty, but declared: otherwise the base fields would be drawn twice in the inspector.
         */
        OPAAX_PROPERTIES(UIPanel)

        OpaaxStringID GetTypeName() const noexcept override { return OPAAX_ID("UIPanel"); }
    };
}
