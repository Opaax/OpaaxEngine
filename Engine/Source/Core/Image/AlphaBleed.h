#pragma once

#include "Core/OpaaxTypes.h"

namespace Opaax
{
    /**
     * Gives every fully transparent texel the colour of the nearest texel that is not, its alpha
     * staying 0. A decoded image keeps any colour (often black) under its transparent texels; a
     * filtered read at a sprite's edge, or a mipmap level, blends that colour in and draws a dark
     * line around the sprite. Nothing changes where the image is read texel for texel.
     * An image with no transparent texel, or with nothing else, is left as it is.
     * @param InOutRGBA InWidth * InHeight * 4 bytes
     * @return How many texels got a colour
     */
    Uint32 BleedAlphaEdges(Uint8* InOutRGBA, Uint32 InWidth, Uint32 InHeight);
}
