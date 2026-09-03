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
         * The UI typeface, as a mount or asset-relative path. EMPTY keeps the toolkit's own default
         * (ImGui's ProggyClean, which is ASCII-only).
         */
        OpaaxString UIFontPath { "/Engine/Fonts/Roboto/roboto-latin-400-normal.ttf" };

        /**
         * Faces merged into the primary so the UI can DISPLAY the scripts the editor can author.
         * Google ships Roboto subsetted, so covering Greek and Cyrillic is a list of files rather
         * than one.
         */
        TDynArray<OpaaxString> UIFontFallbacks
        {
            OpaaxString("/Engine/Fonts/Roboto/roboto-latin-ext-400-normal.ttf"),
            OpaaxString("/Engine/Fonts/Roboto/roboto-greek-400-normal.ttf"),
            OpaaxString("/Engine/Fonts/Roboto/roboto-cyrillic-400-normal.ttf"),
            OpaaxString("/Engine/Fonts/Roboto/roboto-vietnamese-400-normal.ttf"),
        };

        float UIFontSizePx { 16.f };

        // EVERY field goes in both lists. One left out of the json macro is not a smaller config —
        // it is a field that silently never persists (WindowBackground was, until 2026-09-01).
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(EditorImguiConfigData,
            WindowPadding,
            TextColor,
            TextDisabledColor,
            WindowBackground,
            UIFontPath,
            UIFontFallbacks,
            UIFontSizePx)

        // UIFontFallbacks is the ONE exception to "every field in both lists": it is a TDynArray and
        // no property drawer draws a list, the same split SpriteSheetData makes for its frames. It
        // persists and is hand-editable; it simply has no widget.
        //
        // The font is applied once, when the UI comes up, so both drawn fields say so — the
        // NeedRestart flag exists for exactly this and the drawer prints "(restart)" beside them.
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
