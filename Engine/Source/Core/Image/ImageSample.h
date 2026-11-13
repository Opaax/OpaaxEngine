#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax::ImageSample
{
    // =============================================================================
    // ImageSample — what a part of an image holds, in a few numbers: how many different colours,
    //   their mean, the least and most of each channel. A flat area has 1 colour; a hard edge
    //   between two colours has 2; a smoothed edge has more. Lets a script check a picture
    //   without a person looking at it.
    // =============================================================================

    /** A part of an image, in pixels from its top-left corner. */
    struct Rect
    {
        Int32 X      = 0;
        Int32 Y      = 0;
        Int32 Width  = 0;
        Int32 Height = 0;
    };

    /** What Measure found. Channels are red, green, blue, alpha, each 0 to 255. */
    struct Result
    {
        /** The part read: the asked one, cut to the image. */
        Rect  Area;

        /** Different RGBA values in the area. */
        Uint32 Colors = 0;

        double Mean[4] = {};
        Uint8  Min[4]  = {};
        Uint8  Max[4]  = {};
    };

    /**
     * Measures a part of an RGBA image.
     * @param InRGBA   InWidth * InHeight * 4 bytes, rows from top to bottom
     * @param InArea   The part to read; what lies past the image is cut off
     * @param OutError Why it failed, with what to give instead
     * @return False for no pixels, or an area with nothing inside the image
     */
    bool Measure(const Uint8* InRGBA, Uint32 InWidth, Uint32 InHeight, const Rect& InArea,
                 Result& OutResult, OpaaxString& OutError);
}
