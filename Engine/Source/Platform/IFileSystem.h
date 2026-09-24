#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // IFileSystem — the engine's file-system facility, owned BY VALUE by every IPlatform
    //   implementation and handed out through IPlatform::GetFileSystem(). WindowsPlatform holds a
    //   WindowsFileSystem; the accessor's `const IFileSystem&` return means no call site ever knows.
    //
    //   THREE PRIMITIVES ARE VIRTUAL — creating, testing and listing are what an OS actually differs
    //   about. GetPathIfNCreate is NOT: it is policy (create, then log through whichever logger exists
    //   yet) expressed in terms of the primitives, so no platform re-implements it.
    //
    //   PATHS ARE UTF-8, always, in both directions. That is not a free choice — the platform layer
    //   already emits UTF-8 (WindowsPlatform::GetExecutablePath converts through CP_UTF8), so an
    //   implementation that hands those bytes to a narrow OS/CRT call inherits that call's encoding
    //   guess. On MSVC std::filesystem::path(const char*) decodes with the ANSI code page, which
    //   silently disagrees with UTF-8 for every non-ASCII path — see WindowsFileSystem, which converts
    //   explicitly, and the FileSystemTests case that pins it.
    //
    //   STATELESS, so every operation is const: a caller holding the platform's `const IFileSystem&`
    //   reaches the whole surface, and no accessor has to be widened to make the facility usable.
    //
    //   NOTHING HERE THROWS. Every call reports failure by return value (the idiom
    //   Core/IO/FileIO.cpp already follows) — a missing directory or an unreadable entry is an
    //   ordinary answer, not an exception a caller must remember to catch.
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
        // Platform primitives
        // =============================================================================
    public:
        /**
         * Create every missing directory along the specified path.
         *
         * @param InPath UTF-8 path for which directories need to be created.
         * @return true if the directory EXISTS once the call returns — including when it already did.
         *   (std::filesystem::create_directories reports false in that case, which callers never mean.)
         */
        virtual bool CreateDirectories(const OpaaxString& InPath) const = 0;

        /**
         * Check if the specified path exists.
         *
         * @param InPath UTF-8 path to check for existence.
         * @return true if the path exists, false otherwise (including on any query error).
         */
        virtual bool IsPathExist(const OpaaxString& InPath) const = 0;

        /**
         * List one directory level — no recursion, no sorting, no filtering. Composing a tree, ordering
         * it and deciding what is interesting are the CALLER's job; this is the platform primitive.
         * Entries are APPENDED, so one container can accumulate several listings.
         *
         * @param InDirAbs Absolute UTF-8 path of the directory to list.
         * @param OutEntries Receives one Entry per child, names and paths in UTF-8.
         * @return false if InDirAbs is empty, is not a directory, or could not be read.
         */
        virtual bool ListDirectory(const OpaaxString& InDirAbs, TDynArray<Entry>& OutEntries) const = 0;

        // =============================================================================
        // Shared policy (non-virtual — expressed in terms of the primitives above)
        // =============================================================================
    public:
        /**
         * Get the path after creating all necessary directories along the way.
         *
         * Deliberately NOT virtual: the only platform-specific part is the creation itself, which it
         * delegates. What is left is policy — including the fallback that lets it report a failure
         * during Bootstrap, before ILogger has been provided.
         *
         * @param InPath The input path for which directories need to be created.
         * @return The final path after creating all necessary directories. Empty path if directories cannot be created.
         */
        OpaaxString GetPathIfNCreate(const OpaaxString& InPath) const;

        // =============================================================================
        // Null object
        // =============================================================================
    public:
        /**
         * An inert filesystem: every primitive fails, nothing touches a disk. What a null platform or a
         * test stub holds — both used to own a fully working one, which is a strange thing for a null
         * object to do. Out-of-line so there is ONE instance across the DLL/exe line (I2).
         */
        static const IFileSystem& Null();
    };
}
