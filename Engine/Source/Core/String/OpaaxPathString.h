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

    /**
     * The filename WITH its extension: "…/Maps/Decor.opaaxmap" -> "Decor.opaaxmap".
     *
     * What a document panel puts in its title, where the extension is the part that says which
     * editor you are looking at. `Stem` answers the other question; both live here so a second
     * copy of "find the last slash" cannot drift from this one (I13).
     *
     * @return The whole path when it has no directory part, and an empty view for a trailing slash.
     */
    constexpr OpaaxStringView FileName(OpaaxStringView InPath) noexcept
    {
        const Int32 lSlash = InPath.FindLastOf("/\\");

        return (lSlash < 0) ? InPath : InPath.SubString(static_cast<Uint32>(lSlash) + 1u);
    }

    /**
     * The extension WITH its dot: "…/Maps/Decor.opaaxmap" -> ".opaaxmap". Everything after the last
     * dot, the same rule std::filesystem::path::extension applies — without building an fs::path.
     *
     * `Stem`'s two exclusions apply here too, for the same reasons: a dot in a DIRECTORY is not an
     * extension ("C:/a.b/Maps/Decor" -> ""), and a leading dot is a dotfile, which is all extension
     * and no stem, so it reports neither (".gitignore" -> "").
     *
     * Raw text, NOT comparable: case is untouched here. Comparing two extensions goes through
     * NormalizeExtension (Engine/Subsystems/Resources/ResourceFormat.h), which folds case and interns.
     *
     * @return An empty view when the path has no extension.
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
