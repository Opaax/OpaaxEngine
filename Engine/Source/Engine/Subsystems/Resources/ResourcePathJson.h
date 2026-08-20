#pragma once

#include "Core/String/OpaaxStringJson.h"
#include "Engine/Subsystems/Resources/ResourcePath.h"

namespace Opaax
{
    // =============================================================================
    // The nlohmann bridge for TResourcePath<T>, SPLIT from the type the way LinearColorJson.h and
    // OpaaxTagJson.h are.
    //
    // A BARE STRING on disk — "Textures/Hero.png", not {"Path": "..."}. The C++ type exists to make
    // the reference typed for the compiler and the editor; a file has no use for that, and a nested
    // object would make a hand-edited map noisier for nothing. It also means a field that was an
    // OpaaxString before becoming a TResourcePath reads back unchanged.
    // =============================================================================
    template<typename TResource>
    void to_json(nlohmann::json& InJson, const TResourcePath<TResource>& InValue)
    {
        InJson = InValue.Path;
    }

    template<typename TResource>
    void from_json(const nlohmann::json& InJson, TResourcePath<TResource>& InValue)
    {
        // Throws type_error on a non-string, which MapFactory::Instantiate catches per component
        // (I8) — tolerance lives there, once, not in every field.
        InJson.get_to(InValue.Path);
    }
}
