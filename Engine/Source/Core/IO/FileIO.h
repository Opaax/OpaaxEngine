#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax::FileIO
{
    // =============================================================================
    // FileIO — read/write a whole file. Handles UTF-8 paths. Never throws.
    // =============================================================================

    /**
     * Reads a whole file as text, with LF line ends on every platform (a file saved with CRLF reads
     * the same).
     * @param InAbsPath Absolute UTF-8 path
     * @return The contents, or an empty string if the file is missing or unreadable
     */
    OpaaxString ReadAllText(const OpaaxString& InAbsPath);

    /**
     * Reads a whole file as raw bytes.
     * @param InAbsPath Absolute UTF-8 path
     * @param OutBytes Receives the contents. Untouched on failure.
     * @return False if the file is missing or unreadable
     */
    bool ReadAllBytes(const OpaaxString& InAbsPath, TDynArray<Uint8>& OutBytes);

    /**
     * Writes text to InAbsPath as given, replacing any content: LF line ends stay LF on every platform,
     * so a file saved on Windows and on Linux is the same. Creates missing parent directories.
     * @param InAbsPath Absolute UTF-8 path
     * @param InText Content to write
     * @return False if the file could not be written
     */
    bool WriteAllText(const OpaaxString& InAbsPath, const OpaaxString& InText);

    /**
     * Writes raw bytes to InAbsPath, replacing any content. Creates missing parent directories.
     * @param InAbsPath Absolute UTF-8 path
     * @return False if the file could not be written
     */
    bool WriteAllBytes(const OpaaxString& InAbsPath, const Uint8* InData, Uint64 InSize);
}
