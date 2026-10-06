#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // IFileSystem — file system access, owned by the IPlatform (IPlatform::GetFileSystem()).
    //   Three virtual primitives (create, exists, list); GetPathIfNCreate is built on them.
    //   Paths are always UTF-8. Stateless (every call is const). Never throws.
    // =============================================================================
    class IFileSystem
    {
    public:
        virtual ~IFileSystem() {}

        // =============================================================================
        // Types
        // =============================================================================
    public:
        /**
         * One directory entry. AbsPath uses forward slashes.
         */
        struct Entry
        {
            OpaaxString Name;                  // "Textures" / "Wave01.wave"
            OpaaxString AbsPath;
            bool        bIsDirectory = false;
        };

        // =============================================================================
        // Platform primitives
        // =============================================================================
    public:
        /**
         * Creates every missing directory along InPath.
         * @param InPath UTF-8 path
         * @return True if the directory exists afterwards (also when it already did)
         */
        virtual bool CreateDirectories(const OpaaxString& InPath) const = 0;

        /**
         * @param InPath UTF-8 path
         * @return True if the path exists (false on any error)
         */
        virtual bool IsPathExist(const OpaaxString& InPath) const = 0;

        /**
         * Lists one directory level (no recursion, sorting or filtering). Entries are appended.
         * @param InDirAbs Absolute UTF-8 directory path
         * @param OutEntries Receives one Entry per child
         * @return False if InDirAbs is empty, not a directory, or unreadable
         */
        virtual bool ListDirectory(const OpaaxString& InDirAbs, TDynArray<Entry>& OutEntries) const = 0;

        // =============================================================================
        // Shared (non-virtual, built on the primitives)
        // =============================================================================
    public:
        /**
         * Creates the directories of InPath and returns it.
         * @return InPath, or an empty path if the directories cannot be created
         */
        OpaaxString GetPathIfNCreate(const OpaaxString& InPath) const;

        // =============================================================================
        // Null object
        // =============================================================================
    public:
        /**
         * A file system where every call fails. Defined in the .cpp (one instance).
         */
        static const IFileSystem& Null();
    };
}
