#pragma once

#include <entt/entt.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/String/OpaaxStringView.hpp"

namespace Opaax
{
    // =============================================================================
    // DeriveTypeLeafName<T> — the type's name without namespace: "Opaax::QuadComponent" -> "QuadComponent".
    //   Also strips MSVC's "class "/"struct " prefix. Used as the default saved name of a type, so
    //   renaming the C++ type renames it in files too.
    // =============================================================================
    template<typename T>
    OpaaxStringID DeriveTypeLeafName()
    {
        OpaaxStringView lName = entt::type_name<T>::value();

        for (const OpaaxStringView lKeyword : {"class ", "struct ", "enum ", "union "})
        {
            if (lName.StartsWith(lKeyword))
            {
                lName.RemovePrefix(lKeyword.GetLength());
                break;
            }
        }

        const Int32 lSeparator = lName.FindLast("::");
        const OpaaxStringView lLeaf = (lSeparator < 0)
                                          ? lName
                                          : lName.SubString(static_cast<Uint32>(lSeparator) + 2);

        return OpaaxStringID(lLeaf.ToString());
    }
}
