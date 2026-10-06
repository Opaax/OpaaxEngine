#pragma once

#include "Core/String/OpaaxStringJson.h"
#include "Engine/Subsystems/Resources/ResourcePath.h"

namespace Opaax
{
    // =============================================================================
    // JSON for TResourcePath<T>: a plain string ("Textures/Hero.png").
    // =============================================================================
    template<typename TResource, EResourceLoad TLoad>
    void to_json(nlohmann::json& InJson, const TResourcePath<TResource, TLoad>& InValue)
    {
        InJson = InValue.Path;
    }

    template<typename TResource, EResourceLoad TLoad>
    void from_json(const nlohmann::json& InJson, TResourcePath<TResource, TLoad>& InValue)
    {
        // Throws type_error on a non-string (MapFactory::Instantiate catches it).
        InJson.get_to(InValue.Path);
    }
}
