#pragma once

#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // EUIInputMode — Unreal's three modes, held by the UI tenant (there is no PlayerController
    //   tier; the session is where the mode lives). It decides what the raw feed reaches.
    // =============================================================================
    enum class EUIInputMode : Uint8
    {
        GameOnly,    // the UI routes nothing; the mapping sees everything (the HUD)
        UIOnly,      // the UI routes; the mapping is muted whole (a modal menu)
        GameAndUI    // the UI routes first; what it handled is consumed before the mapping (default)
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
