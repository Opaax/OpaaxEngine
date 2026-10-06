#pragma once

#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // EUIInputMode — what the UI receives (like Unreal's input modes).
    // =============================================================================
    enum class EUIInputMode : Uint8
    {
        GameOnly,    // UI gets nothing; the game gets everything (HUD)
        UIOnly,      // UI gets everything; the game nothing (modal menu)
        GameAndUI    // UI first; what it handles does not reach the game (default)
    };

    inline const char* ToString(const EUIInputMode InMode) noexcept
    {
        switch (InMode)
        {
            case EUIInputMode::GameOnly:  return "GameOnly";
            case EUIInputMode::UIOnly:    return "UIOnly";
            case EUIInputMode::GameAndUI: return "GameAndUI";
        }
        return "GameAndUI";
    }
}
