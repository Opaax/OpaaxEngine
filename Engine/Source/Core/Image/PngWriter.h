#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax::PngWriter
{
    // =============================================================================
    // PngWriter — 8-bit RGB or RGBA images as PNG files (frame captures, generated textures).
    //   The pixel data is stored, not compressed: larger files, but exact and fast to write.
    // =============================================================================

    /**
     * The PNG file's bytes.
     * @param InChannels 3 (RGB) or 4 (RGBA)
     * @param InPixels   InWidth * InHeight * InChannels bytes, rows from top to bottom
     * @return Empty for a zero size, another channel count, or no pixels
     */
    TDynArray<Uint8> Encode(Uint32 InWidth, Uint32 InHeight, Uint32 InChannels, const Uint8* InPixels);

    /**
     * Encodes, then writes InAbsPath (creating its folders).
     * @return False when the image is invalid or the file cannot be written
     */
    bool Write(const OpaaxString& InAbsPath, Uint32 InWidth, Uint32 InHeight, Uint32 InChannels, const Uint8* InPixels);
}
