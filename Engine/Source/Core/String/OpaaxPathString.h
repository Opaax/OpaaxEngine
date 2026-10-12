#pragma once

#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringView.hpp"

namespace Opaax::PathString
{
    // =============================================================================
    // Path string helpers. Text only (no filesystem access), UTF-8 safe.
    // Stem, FileName and Extension return views into the argument (ToString() keeps one); the
    // others build a new path, with forward slashes, by the platform's rules (a drive letter is a
    // root on Windows only). FileSystem has the questions for the disk.
    // =============================================================================

    /**
     * InA, then InB below it: ("Maps", "Decor.opaaxmap") -> "Maps/Decor.opaaxmap". An absolute
     * InB replaces InA.
     */
    OpaaxString Join(OpaaxStringView InA, OpaaxStringView InB);

    /** The directory part: ".../Maps/Decor.opaaxmap" -> ".../Maps". Empty without a directory. */
    OpaaxString Parent(OpaaxStringView InPath);

    /** The same path with forward slashes between its names (on Windows, a backslash is one too). */
    OpaaxString Generic(OpaaxStringView InPath);

    /** "." and ".." resolved, without the disk: "a/./b/../c" -> "a/c". */
    OpaaxString Normalize(OpaaxStringView InPath);

    /** A path from a root: "/x" on Linux and macOS, "C:/x" on Windows (where "/x" is on the current drive). */
    bool IsAbsolute(OpaaxStringView InPath);

    /**
     * InPath seen from InBase, without the disk: ("a/b/c", "a") -> "b/c", ("a", "a/b") -> "..".
     * @return Empty when one cannot be reached from the other (another drive)
     */
    OpaaxString RelativeTo(OpaaxStringView InPath, OpaaxStringView InBase);

    /** InPath with InExtension in place of its own: ("a/b.json", ".out.json") -> "a/b.out.json". */
    OpaaxString ReplaceExtension(OpaaxStringView InPath, OpaaxStringView InExtension);

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
