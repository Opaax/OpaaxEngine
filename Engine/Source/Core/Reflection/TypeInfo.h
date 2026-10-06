#pragma once

#include <string_view>

#include "Core/Hash/OpaaxHash.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/String/OpaaxStringView.hpp"

namespace Opaax
{
    // =============================================================================
    // Type identity, owned by the engine (no vendor). Built from the compiler's signature of a
    //   function instantiated for T, so it is the same in every translation unit and every run.
    // =============================================================================

    /** A type's id: a hash of its signature. Stable across modules and runs; not a dense index. */
    using TypeId = Uint64;

    /** The compiler's signature of this function for T. Unique per type. */
    template<typename T>
    constexpr std::string_view TypeSignature() noexcept
    {
#if defined(_MSC_VER) && !defined(__clang__)
        return __FUNCSIG__;
#else
        return __PRETTY_FUNCTION__;
#endif
    }

    /**
     * T's full name as the compiler spells it. MSVC keeps the "struct "/"class " keyword
     * ("struct Opaax::QuadComponent"); clang/gcc do not.
     */
    template<typename T>
    constexpr std::string_view TypeNameOf() noexcept
    {
        constexpr std::string_view lSignature = TypeSignature<T>();

#if defined(_MSC_VER) && !defined(__clang__)
        // "... __cdecl Opaax::TypeSignature<struct Opaax::QuadComponent>(void) noexcept"
        constexpr std::string_view lOpen  = "TypeSignature<";
        constexpr Uint64           lStart = lSignature.find(lOpen) + lOpen.size();
        constexpr Uint64           lEnd   = lSignature.rfind(">(void)");
#else
        // "... TypeSignature() [T = Opaax::QuadComponent]" (clang) or "[with T = ...; ...]" (gcc)
        constexpr std::string_view lOpen  = "T = ";
        constexpr Uint64           lStart = lSignature.find(lOpen) + lOpen.size();
        constexpr Uint64           lEnd   = lSignature.find_first_of(";]", lStart);
#endif

        return lSignature.substr(lStart, lEnd - lStart);
    }

    template<typename T>
    constexpr TypeId TypeIdOf() noexcept
    {
        return OpaaxHash::Hash64(TypeSignature<T>());
    }

    /**
     * T's name without namespace or keyword: "Opaax::QuadComponent" -> "QuadComponent". The default
     * saved name of a type (components, data assets), so renaming the C++ type renames it in files too.
     */
    template<typename T>
    OpaaxStringID DeriveTypeLeafName()
    {
        OpaaxStringView lName = TypeNameOf<T>();

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
