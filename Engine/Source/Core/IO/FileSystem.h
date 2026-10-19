#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax::FileSystem
{
    // =============================================================================
    // FileSystem — files and directories on disk: exist, create, list, copy, rename, remove.
    //   Paths are UTF-8 (forward slashes in what comes back). Never throws: a failure is false or
    //   an empty path. Whole-file reads and writes are FileIO; path text is PathString.
    // =============================================================================

    /** One directory entry. AbsPath uses forward slashes. */
    struct Entry
    {
        OpaaxString Name;                  // "Textures" / "Wave01.wave"
        OpaaxString AbsPath;
        bool        bIsDirectory = false;
    };

    // =============================================================================
    // Questions
    // =============================================================================
    /** @return True if something (a file, a directory...) is at InPath */
    bool Exists(const OpaaxString& InPath);

    /** @return True if InPath is a regular file */
    bool IsFile(const OpaaxString& InPath);

    /** @return True if InPath is a directory */
    bool IsDirectory(const OpaaxString& InPath);

    /**
     * Lists one directory level (no recursion, sorting or filtering). Entries are appended; an
     * entry that cannot be read is skipped.
     * @return False if InDir is empty, not a directory, or unreadable
     */
    bool ListDirectory(const OpaaxString& InDir, TDynArray<Entry>& OutEntries);

    // =============================================================================
    // Changes
    // =============================================================================
    /**
     * Creates every missing directory along InPath.
     * @return True if InPath is a directory afterwards (also when it already was)
     */
    bool CreateDirectories(const OpaaxString& InPath);

    /**
     * Deletes a file, or an empty directory.
     * @return True if something was deleted
     */
    bool Remove(const OpaaxString& InPath);

    /**
     * Deletes InPath and everything under it.
     * @return False if something could not be deleted (a missing path is not a failure)
     */
    bool RemoveAll(const OpaaxString& InPath);

    /**
     * Moves or renames InFrom to InTo, replacing a file already at InTo. On one volume a reader
     * sees the old file or the new one, never half of it.
     */
    bool Rename(const OpaaxString& InFrom, const OpaaxString& InTo);

    /** Copies the file InFrom to InTo; bInOverwrite replaces a file already at InTo. */
    bool Copy(const OpaaxString& InFrom, const OpaaxString& InTo, bool bInOverwrite = true);

    // =============================================================================
    // Paths
    // =============================================================================
    /** The process's working directory. */
    OpaaxString WorkingDirectory();

    /** The system's directory for temporary files. */
    OpaaxString TempDirectory();

    /** InPath made absolute against the working directory, without touching the disk otherwise. */
    OpaaxString MakeAbsolute(const OpaaxString& InPath);

    /**
     * InPath absolute, with "." and ".." resolved and links followed as far as it exists (the rest,
     * a file not written yet, is kept as written).
     * @return Empty on failure
     */
    OpaaxString MakeCanonical(const OpaaxString& InPath);
}
