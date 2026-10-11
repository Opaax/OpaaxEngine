#pragma once

#if OPAAX_WITH_EDITOR

#include "Core/OpaaxString.hpp"
#include "Core/OpaaxStringID.hpp"
#include "Assets/AssetManifest.h"

namespace Opaax::Editor
{
    /**
     * @struct AssetBrowserFilter
     * Holds the active filter state for the asset browser.
     * Matches() returns true if a descriptor passes all active filters.
     */
    struct AssetBrowserFilter
    {
        OpaaxString   Text;
        OpaaxStringID TypeID;  // empty = show all types

        bool Matches(const AssetDescriptor& InDesc) const
        {
            if (TypeID.IsValid() && InDesc.Type != TypeID)
            {
                return false;
            }

            if (!Text.IsEmpty())
            {
                const OpaaxString lIDStr  = InDesc.ID.ToString().ToLower();
                const OpaaxString lFilter = Text.ToLower();
                if (lIDStr.Find(lFilter.CStr()) == -1) { return false; }
            }

            return true;
        }
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
