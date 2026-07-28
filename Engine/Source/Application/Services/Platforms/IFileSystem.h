#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // IFileSystem — the engine's file-system facility, owned BY VALUE by every IPlatform
    //   implementation and handed out through IPlatform::GetFileSystem().
    //
    //   The bodies are std::filesystem today and the methods are NOT virtual, because there is exactly
    //   one implementation. A platform-specific one (WindowsFileSystem) is planned (user, 2026-07-27):
    //   when it lands these become virtual and each platform holds its OWN concrete type by value —
    //   the accessor's `const IFileSystem&` return already supports that with no call-site change.
    //
    //   STATELESS, so every operation is const: a caller holding the platform's `const IFileSystem&`
    //   reaches the whole surface, and no accessor has to be widened to make the facility usable.
    //
    //   NOTHING HERE THROWS. Every call uses the std::error_code overload and reports failure by
    //   return value (the idiom Core/Config/ConfigIO.cpp already follows) — a missing directory or an
    //   unreadable entry is an ordinary answer, not an exception a caller must remember to catch.
    // =============================================================================
    class OPAAX_API IFileSystem
    {
    public:
        virtual ~IFileSystem() {}

        // =============================================================================
        // Types
        // =============================================================================
    public:
        /**
         * @struct Entry
         * One child of a listed directory. AbsPath uses forward slashes on every platform, so paths
         * built from it compare and display consistently.
         */
        struct Entry
        {
            OpaaxString Name;                  // "Textures" / "Wave01.wave"
            OpaaxString AbsPath;
            bool        bIsDirectory = false;
        };

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Create every missing directory along the specified path.
         *
         * @param InPath The path for which directories need to be created.
         * @return true if the directory EXISTS once the call returns — including when it already did.
         *   (std::filesystem::create_directories reports false in that case, which callers never mean.)
         */
        bool CreateDirectories(const OpaaxString& InPath) const;

        /**
         * Check if the specified path exists.
         *
         * @param InPath The path to check for existence.
         * @return true if the path exists, false otherwise (including on any query error).
         */
        bool IsPathExist(const OpaaxString& InPath) const;

        /**
         * Get the path after creating all necessary directories along the way.
         *
         * @param InPath The input path for which directories need to be created.
         * @return The final path after creating all necessary directories. Empty path if directories cannot be created.
         */
        OpaaxString GetPathIfNCreate(const OpaaxString& InPath) const;

        /**
         * List one directory level — no recursion, no sorting, no filtering. Composing a tree, ordering
         * it and deciding what is interesting are the CALLER's job; this is the platform primitive.
         * Entries are APPENDED, so one container can accumulate several listings.
         *
         * @param InDirAbs Absolute path of the directory to list.
         * @param OutEntries Receives one Entry per child.
         * @return false if InDirAbs is empty, is not a directory, or could not be read.
         */
        bool ListDirectory(const OpaaxString& InDirAbs, TDynArray<Entry>& OutEntries) const;
    };
}
