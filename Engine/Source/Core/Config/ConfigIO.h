#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxString.hpp"

namespace Opaax::ConfigIO
{
    // =============================================================================
    // ConfigIO — tolerant text file-IO shared by every config. Kept out-of-line so
    // TConfig.h (widely included) pulls in no <filesystem>/<fstream>.
    // =============================================================================

    // Read the whole file as text. Returns an empty string if the file is missing
    // or cannot be opened (callers treat empty as "no config yet").
    OPAAX_API OpaaxString ReadText(const OpaaxString& InAbsPath);

    // Write text to InAbsPath, creating any missing parent directories. Never throws.
    OPAAX_API bool        WriteText(const OpaaxString& InAbsPath, const OpaaxString& InText);
}
