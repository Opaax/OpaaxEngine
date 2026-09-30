#pragma once

#include <nlohmann/json.hpp>

#include "Core/Color/LinearColorJson.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringJson.h"

namespace Opaax
{
    struct EditorImguiConfigData
    {
        Vector2F WindowPadding {10.0f, 10.0f};

        LinearColor TextColor           {1.f, 1.f, 1.f, 1.0f};
        LinearColor TextDisabledColor   {.6f, .6f, .6f, 1.0f};

        LinearColor WindowBackground    {.0f, .0f, .0f, .85f};

        /**
         * The UI font, as a mount or asset-relative path. Empty keeps ImGui's default (ASCII only).
         */
        OpaaxString UIFontPath { "/Engine/Fonts/Roboto/roboto-latin-400-normal.ttf" };

        /**
         * Faces merged into the primary so the UI can show other scripts (e.g. Greek and Cyrillic, often
         * shipped as separate files).
         */
        TDynArray<OpaaxString> UIFontFallbacks
        {
            OpaaxString("/Engine/Fonts/Roboto/roboto-latin-ext-400-normal.ttf"),
            OpaaxString("/Engine/Fonts/Roboto/roboto-greek-400-normal.ttf"),
            OpaaxString("/Engine/Fonts/Roboto/roboto-cyrillic-400-normal.ttf"),
            OpaaxString("/Engine/Fonts/Roboto/roboto-vietnamese-400-normal.ttf"),
        };

        float UIFontSizePx { 16.f };

        // Every field goes in both lists: a field missing from the json macro is silently never saved.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(EditorImguiConfigData,
            WindowPadding,
            TextColor,
            TextDisabledColor,
            WindowBackground,
            UIFontPath,
            UIFontFallbacks,
            UIFontSizePx)

        // Exception: UIFontFallbacks is a list and no property drawer draws lists. It is saved and can be
        // edited by hand; it just has no widget.
        // The font is applied once at startup, so both font fields are marked NeedRestart.
        OPAAX_PROPERTIES(EditorImguiConfigData,
                        OPAAX_PROP(WindowPadding),
                        OPAAX_PROP(TextColor),
                        OPAAX_PROP(TextDisabledColor),
                        OPAAX_PROP(WindowBackground),
                        OPAAX_PROP(UIFontPath).SetFlags(EPropertyFlags::NeedRestart)
                                              .SetTooltip("The editor's own typeface.\n"
                                                          "Empty falls back to ImGui's ASCII-only default."),
                        OPAAX_PROP(UIFontSizePx).SetRange(8.f, 32.f)
                                                .SetFlags(EPropertyFlags::NeedRestart)
                      )
    };
}
