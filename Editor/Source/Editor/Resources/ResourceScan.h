#pragma once

#include "Core/OpaaxTypes.h"                // TDynArray, Uint64
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"

namespace Opaax
{
    class IFileSystem;
}

namespace Opaax::Editor
{
    // =============================================================================
    // ResourceFile — one file on disk, as the browser sees it. Deliberately NOT a resource handle:
    //   M2d is a FILE browser (overview §3.5), so nothing here is loaded, resolved or reference-counted.
    //   The GUID-backed catalog is M3's job, when scene files need stable ids.
    // =============================================================================
    struct ResourceFile
    {
        OpaaxString   Name;        // "Wave01.wave"
        OpaaxString   RelPath;     // "Waves/Wave01.wave" — root-relative, forward slashes
        OpaaxString   AbsPath;
        OpaaxStringID Extension;   // ".wave", normalized; INVALID for an extension-less file
    };

    // =============================================================================
    // ResourceFolder — one directory level. Children are sorted at scan time, so every view renders
    //   the same order and a rescan cannot reshuffle the list under the user.
    // =============================================================================
    struct ResourceFolder
    {
        OpaaxString               Name;
        OpaaxString               RelPath;   // "" for a root's own folder
        TDynArray<ResourceFolder> Folders;
        TDynArray<ResourceFile>   Files;
    };

    // =============================================================================
    // ResourceRoot — one browsable tree with a display identity. The panel owns a small array of these
    //   (Project + Editor today), which is what keeps "add another root" a one-line change.
    // =============================================================================
    struct ResourceRoot
    {
        OpaaxStringID  Label;               // "Project" | "Editor" — compared when walking the breadcrumb
        OpaaxString    AbsPath;
        ResourceFolder Tree;
        Uint64         FileCount   = 0;
        Uint64         FolderCount = 0;
        bool           bExists     = false; // false => the directory is simply not there (a normal state)
    };

    /**
     * Turn a raw file extension into the id both sides of the lookup agree on: lower-cased (Windows
     * paths are case-insensitive, so ".PNG" and ".png" are one type) with a guaranteed leading dot.
     * The ONE place an extension becomes comparable — the registry and the scanner both come here.
     *
     * @param InExtension Raw extension, with or without its dot ("wave", ".WAVE").
     * @return The normalized interned id, or an INVALID id when InExtension is empty.
     */
    OpaaxStringID NormalizeExtension(const OpaaxString& InExtension);

    /**
     * Rebuild InOutRoot.Tree from disk, in full. Sorted, recursive, and driven entirely through
     * IFileSystem::ListDirectory — this file includes no <filesystem> of its own.
     *
     * @param InFileSystem The platform's file system (IPlatform::GetFileSystem).
     * @param InOutRoot Label + AbsPath are read; Tree, the counts and bExists are written.
     * @return false when AbsPath is not an existing directory — InOutRoot is still left in a valid, empty state.
     */
    bool ScanRoot(const IFileSystem& InFileSystem, ResourceRoot& InOutRoot);
}
