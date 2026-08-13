#pragma once

#include "Core/String/OpaaxStringView.hpp"

namespace Opaax::PathString
{
    // =============================================================================
    // Byte-wise surgery on a path STRING — never a filesystem touch.
    //
    // '/', '\' and '.' are ASCII, so scanning for them is UTF-8 safe on any engine string (I7)
    // without building an fs::path, which on MSVC would decode the bytes as ANSI and answer for a
    // different file than the caller named. That is the reason the callers here were hand-rolled;
    // this header is where the rule lives now, once.
    //
    // Everything returns a VIEW INTO the argument: it borrows, allocates nothing, and dies with the
    // path it was given. Call ToString() to keep it.
    //
    // Path COMPOSITION is IPaths' job, above Application. This is text.
    // =============================================================================

    /**
     * The filename without its extension: "…/Maps/Decor.opaaxmap" -> "Decor".
     *
     * A dot in a DIRECTORY does not count ("C:/a.b/Maps/Decor" -> "Decor"), and a leading dot is a
     * dotfile, which is all extension and no stem (".gitignore" -> "").
     *
     * @return An empty view when the path has no stem — a bare directory, or nothing at all.
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
} // namespace Opaax::PathString
