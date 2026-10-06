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
    // ResourceFile — one file on disk, as the browser sees it. Not a resource handle: nothing here is
    //   loaded or reference-counted.
    // =============================================================================
    struct ResourceFile
    {
        OpaaxString   Name;        // "Wave01.wave"
        OpaaxString   RelPath;     // "Waves/Wave01.wave" — root-relative, forward slashes
        OpaaxString   AbsPath;
        OpaaxStringID Extension;   // ".wave", normalized; INVALID for an extension-less file
    };

    // =============================================================================
    // ResourceFolder — one directory level. Children are sorted at scan time, so a rescan does not
    //   reshuffle the list.
    // =============================================================================
    struct ResourceFolder
    {
        OpaaxString               Name;
        OpaaxString               RelPath;   // "" for a root's own folder
        TDynArray<ResourceFolder> Folders;
        TDynArray<ResourceFile>   Files;
    };

    // =============================================================================
    // ResourceRoot — one browsable tree with a display name (Project, Editor).
    // =============================================================================
    struct ResourceRoot
    {
        OpaaxStringID  Label;               // "Project" | "Editor"
        OpaaxString    AbsPath;
        ResourceFolder Tree;
        Uint64         FileCount   = 0;
        Uint64         FolderCount = 0;
        bool           bExists     = false; // false: the directory does not exist (normal)
    };

    // NormalizeExtension lives in the engine (Resources/ResourceFormat.h), so the
    // scanner and the resource registry use the same function.

    /**
     * Rebuilds InOutRoot.Tree from disk: sorted, recursive, through IFileSystem::ListDirectory.
     * @param InFileSystem The platform's file system (IPlatform::GetFileSystem)
     * @param InOutRoot Label and AbsPath are read; Tree, the counts and bExists are written
     * @return False when AbsPath is not an existing directory (InOutRoot is left valid and empty)
     */
    bool ScanRoot(const IFileSystem& InFileSystem, ResourceRoot& InOutRoot);
}
