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
     * Reads a whole file as text.
     * @param InAbsPath Absolute UTF-8 path
     * @return The contents, or an empty string if the file is missing or unreadable
     */
    OPAAX_API OpaaxString ReadAllText(const OpaaxString& InAbsPath);

    /**
     * Reads a whole file as raw bytes.
     * @param InAbsPath Absolute UTF-8 path
     * @param OutBytes Receives the contents. Untouched on failure.
     * @return False if the file is missing or unreadable
     */
    OPAAX_API bool ReadAllBytes(const OpaaxString& InAbsPath, TDynArray<Uint8>& OutBytes);

    /**
     * Writes text to InAbsPath, replacing any content. Creates missing parent directories.
     * @param InAbsPath Absolute UTF-8 path
     * @param InText Content to write
     * @return False if the file could not be written
     */
    OPAAX_API bool WriteAllText(const OpaaxString& InAbsPath, const OpaaxString& InText);
}
