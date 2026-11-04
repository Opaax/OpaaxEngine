#pragma once

#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // DisplayMode — a size and refresh rate a screen can show in fullscreen: what a settings menu's
    //   resolution list offers. Pure helpers beside it, so the choices are tested without a screen.
    // =============================================================================
    struct DisplayMode
    {
        Uint32 Width       = 0;
        Uint32 Height      = 0;
        Uint32 RefreshRate = 0;   // hertz; 0 when the screen does not say

        bool operator==(const DisplayMode& InOther) const noexcept
        {
            return Width == InOther.Width && Height == InOther.Height && RefreshRate == InOther.RefreshRate;
        }
    };

    /** Where a screen is, in the desktop's coordinates, and its size at its current mode. */
    struct ScreenArea
    {
        Int32  X      = 0;
        Int32  Y      = 0;
        Uint32 Width  = 0;
        Uint32 Height = 0;
    };

    /**
     * InModes smallest first (by size, then refresh rate), each once: a screen lists a mode once for
     * each colour depth.
     */
    TDynArray<DisplayMode> SortDisplayModes(TDynArray<DisplayMode> InModes);

    /**
     * The mode of InModes a fullscreen window of InWidth x InHeight gets: that size at its highest
     * refresh rate; without it, the closest size (then the higher refresh rate).
     * @return {InWidth, InHeight, 0} when InModes is empty
     */
    DisplayMode ChooseDisplayMode(const TDynArray<DisplayMode>& InModes, Uint32 InWidth, Uint32 InHeight);

    /** The screen of InScreens holding the point InX, InY (a window's centre), or -1 when none does. */
    Int32 FindScreenAt(const TDynArray<ScreenArea>& InScreens, Int32 InX, Int32 InY);
}
