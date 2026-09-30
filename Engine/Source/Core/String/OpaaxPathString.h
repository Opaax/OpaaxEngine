#pragma once

#include "Core/String/OpaaxStringView.hpp"

namespace Opaax::PathString
{
    // =============================================================================
    // Path string helpers. Text only (no filesystem access), UTF-8 safe.
    // Results are views into the argument; call ToString() to keep one.
    // =============================================================================

    /**
     * File name without extension: ".../Maps/Decor.opaaxmap" -> "Decor".
     * Dots in directories are ignored; a dotfile has no stem (".gitignore" -> "").
     * @return Empty if the path has no stem
     */
    constexpr OpaaxStringView Stem(OpaaxStringView InPath) noexcept
    {
        const Int32  lSlash = InPath.FindLastOf("/\\");
        const Uint32 lStart = (lSlash < 0) ? 0u : static_cast<Uint32>(lSlash) + 1u;

        const Int32  lDot = InPath.FindLast('.');
        const Uint32 lEnd = (lDot < 0 || static_cast<Uint32>(lDot) < lStart)
                                ? InPath.GetLength()
                                : static_cast<Uint32>(lDot);

        return InPath.SubString(lStart, lEnd - lStart);
    }

    /**
     * File name with extension: ".../Maps/Decor.opaaxmap" -> "Decor.opaaxmap".
     * @return The whole path if it has no directory; empty for a trailing slash
     */
    constexpr OpaaxStringView FileName(OpaaxStringView InPath) noexcept
    {
        const Int32 lSlash = InPath.FindLastOf("/\\");

        return (lSlash < 0) ? InPath : InPath.SubString(static_cast<Uint32>(lSlash) + 1u);
    }

    /**
     * Extension with its dot: ".../Maps/Decor.opaaxmap" -> ".opaaxmap".
     * Dots in directories are ignored; a dotfile has no extension. Case is kept as is
     * (compare with NormalizeExtension).
     * @return Empty if the path has no extension
     */
    constexpr OpaaxStringView Extension(OpaaxStringView InPath) noexcept
    {
        const Int32  lSlash = InPath.FindLastOf("/\\");
        const Uint32 lStart = (lSlash < 0) ? 0u : static_cast<Uint32>(lSlash) + 1u;

        const Int32 lDot = InPath.FindLast('.');
        if (lDot < 0 || static_cast<Uint32>(lDot) <= lStart)
        {
            return {};
        }

        return InPath.SubString(static_cast<Uint32>(lDot));
    }
} // namespace Opaax::PathString
