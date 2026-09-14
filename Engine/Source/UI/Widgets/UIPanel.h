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

        OpaaxStringID GetTypeName() const noexcept override { return OPAAX_ID("UIPanel"); }
    };
}
