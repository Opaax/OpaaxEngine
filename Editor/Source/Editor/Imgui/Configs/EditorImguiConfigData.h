#pragma once

#include <nlohmann/json.hpp>

#include "Core/Color/LinearColorJson.h"
#include "Core/Reflection/OpaaxProperty.h"

namespace Opaax
{
    struct EditorImguiConfigData
    {
        Vector2F WindowPadding {10.0f, 10.0f};
        
        LinearColor TextColor           {1.f, 1.f, 1.f, 1.0f};
        LinearColor TextDisabledColor   {.6f, .6f, .6f, 1.0f};
        
        LinearColor WindowBackground    {.0f, .0f, .0f, .85f};

        // EVERY field goes in both lists. One left out of the json macro is not a smaller config —
        // it is a field that silently never persists (WindowBackground was, until 2026-09-01).
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(EditorImguiConfigData,
            WindowPadding,
            TextColor,
            TextDisabledColor,
            WindowBackground)

        OPAAX_PROPERTIES(EditorImguiConfigData,
                        OPAAX_PROP(WindowPadding),
                        OPAAX_PROP(TextColor),
                        OPAAX_PROP(TextDisabledColor),
                        OPAAX_PROP(WindowBackground)
                      )
    };
}
