#pragma once

#include <nlohmann/json.hpp>

#include "Core/Reflection/OpaaxProperty.h"
#include "Engine/Registries/AutoRegistration.h"

namespace __NAME__
{
    // =============================================================================
    // ExampleComponent — plain data on an entity: editable in the Inspector, saved in map files.
    //   The registration line below is all it takes; delete this file when you have your own.
    // =============================================================================
    struct ExampleComponent
    {
        float Speed = 100.f;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(ExampleComponent, Speed)
        OPAAX_PROPERTIES(ExampleComponent, OPAAX_PROP(Speed).SetRange(0.f, 1000.f))
    };

    OPAAX_REGISTER_COMPONENT(ExampleComponent);
}
