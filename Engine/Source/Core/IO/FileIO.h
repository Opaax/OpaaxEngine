#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax::FileIO
{
    // =============================================================================
    // FileIO — read/write a whole file, once, for everyone.
    //
    //   Four hand-rolled copies of this used to exist (config, project file, shader, binary
    //   resource), and the UTF-8 sweep had to fix each one separately — which is the argument for
    //   there being one. Every entry point goes through Utf8::ToFsPath (**I7**): a stream opened
    //   from OpaaxString::CStr() resolves a DIFFERENT FILE than the caller named, silently.
    //
    //   Deliberately NOT on IFileSystem. That facility is reachable only through the service
    //   locator, and Core (config) and the portable Renderer sit below it — routing them through it
    //   would mean injecting a filesystem into TConfig and the CResource contract. I7 is the
    //   invariant; IFileSystem is one consumer of it.
    //
    //   Out-of-line on purpose: TConfig.hpp is included widely and must not pull in <fstream>.
    //
    //   TOLERANT — nothing throws, nothing partially reports. A missing or unreadable file is an
    //   ordinary answer, given by return value.
    // =============================================================================

    /**
     * Read a whole file as text.
     * @param InAbsPath Absolute UTF-8 path.
     * @return The file's contents, or an EMPTY string if it is missing or unreadable. Callers that
     *   must tell "empty file" from "no file" should ask IFileSystem::IsPathExist first — every
     *   caller today treats both as "nothing configured yet".
     */
    OPAAX_API OpaaxString ReadAllText(const OpaaxString& InAbsPath);

    /**
     * Read a whole file as raw bytes.
     * @param InAbsPath Absolute UTF-8 path.
     * @param OutBytes Receives the contents. Left UNTOUCHED on failure, so a rejected read cannot
     *   corrupt what the caller already held.
     * @return false if the file is missing, unreadable, or its size cannot be determined.
     */
    OPAAX_API bool ReadAllBytes(const OpaaxString& InAbsPath, TDynArray<Uint8>& OutBytes);

    /**
     * Write text to InAbsPath, replacing any existing content.
     *
     * CREATES MISSING PARENT DIRECTORIES — inherited from the config writer this replaced, and what
     * every caller means: "put a file here", not "put a file here if the folder happens to exist".
     *
     * @param InAbsPath Absolute UTF-8 path.
     * @param InText Content to write.
     * @return false if the parents could not be created or the file could not be opened.
     */
    OPAAX_API bool WriteAllText(const OpaaxString& InAbsPath, const OpaaxString& InText);
}
