#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    class UIWidget;

    // =============================================================================
    // UIWidgetNames — names within one widget tree. Game code finds widgets by name (UIWidget::FindByName,
    //   first match, depth first), so a tree where two widgets share a name hides one of them. The editor
    //   names what it adds with NameUniquely; files are checked with FindDuplicates.
    // =============================================================================
    namespace UIWidgetNames
    {
        /** Whether a widget of InTree other than InExcept (may be null) is named InName. */
        bool IsTaken(const UIWidget& InTree, const OpaaxString& InName, const UIWidget* InExcept = nullptr);

        /**
         * Names InNew and its subtree, before it joins InTree, so that no widget has another's name
         * (UniqueName: "Button", "Button 1"...). Unnamed widgets stay unnamed: nothing finds them by name.
         */
        void NameUniquely(const UIWidget& InTree, UIWidget& InNew);

        /** InTree's names held by more than one widget, each once, in tree order. */
        TDynArray<OpaaxString> FindDuplicates(const UIWidget& InTree);
    }
}
