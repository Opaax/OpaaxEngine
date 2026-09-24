#pragma once

#include "Core/String/OpaaxStringID.hpp"
#include "UI/UIWidget.h"

namespace Opaax
{
    /**
     * A container: a rect for children to anchor to, nothing drawn. The canvas root is one.
     * Not hit-testable by default — an empty rect must not swallow a click (Unity's empty RectTransform).
     */
    class OPAAX_API UIPanel final : public UIWidget
    {
    public:
        UIPanel() { bHitTestable = false; }

        /**
         * EMPTY, and declared rather than inherited (**UI18**).
         *
         * Without it `GetProperties()` resolves to `UIWidget`'s, so a panel's inspector would draw
         * the base fields a SECOND time under its own drawer. Every widget type states its own
         * list, even when that list is nothing.
         */
        OPAAX_PROPERTIES(UIPanel)

        OpaaxStringID GetTypeName() const noexcept override { return OPAAX_ID("UIPanel"); }
    };
}
