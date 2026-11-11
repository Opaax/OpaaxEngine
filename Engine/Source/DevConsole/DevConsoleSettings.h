#pragma once

#include <nlohmann/json.hpp>

#include "Core/Color/LinearColor.h"
#include "Core/Color/LinearColorJson.h"
#include "Core/Log/LogLevelNames.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Input/InputCodes.h"
#include "Input/InputKeyNames.h"

namespace Opaax
{
    // =============================================================================
    // ConsoleSettings — the Console section of Engine.config: the dev console's key, size and colours.
    //   Read when a game starts. A ship build has no console, whatever it says.
    // =============================================================================
    struct ConsoleSettings
    {
        /** The key that opens and closes the console: the one left of 1 (` on a US keyboard, ² on a French one). */
        EKeyCode ToggleKey = EKeyCode::GraveAccent;

        /** Pauses the world while the console is open. */
        bool bPauseWorld = false;

        /** The part of the screen the console covers, from the top. */
        float Height = 0.5f;

        /** The text's size, in pixels of a 1080-pixel-tall screen. */
        float FontSize = 22.f;

        /** The most output rows kept. */
        Uint32 MaxLines = 200;

        /** The most lines kept in the history (Up, Down), saved between runs. */
        Uint32 HistorySize = 100;

        /** The lowest level of the log lines the console shows. */
        ELogLevel LogLevel = ELogLevel::Warn;

        // Binary fractions: exact as floats, so Engine.config shows them as written.
        LinearColor BackgroundColor{ 0.015625f, 0.03125f, 0.046875f, 0.875f };
        LinearColor TextColor{ 0.875f, 0.875f, 0.875f, 1.f };
        LinearColor InputColor{ 0.5f, 0.75f, 1.f, 1.f };
        LinearColor WarningColor{ 1.f, 0.75f, 0.25f, 1.f };
        LinearColor ErrorColor{ 1.f, 0.375f, 0.375f, 1.f };

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(ConsoleSettings, ToggleKey, bPauseWorld, Height, FontSize, MaxLines,
                                                    HistorySize, LogLevel, BackgroundColor, TextColor, InputColor,
                                                    WarningColor, ErrorColor)

        OPAAX_PROPERTIES(ConsoleSettings,
                         OPAAX_PROP(ToggleKey).SetTooltip("Opens and closes the console (dev builds only)."),
                         OPAAX_PROP(bPauseWorld).SetTooltip("Pause the world while the console is open."),
                         OPAAX_PROP(Height).SetRange(0.1f, 1.f).SetDragStep(0.01f)
                             .SetTooltip("The part of the screen the console covers, from the top."),
                         OPAAX_PROP(FontSize).SetRange(8.f, 64.f)
                             .SetTooltip("The text's size, in pixels of a 1080-pixel-tall screen."),
                         OPAAX_PROP(MaxLines).SetRange(10.f, 5000.f).SetTooltip("The most output rows kept."),
                         OPAAX_PROP(HistorySize).SetRange(1.f, 1000.f)
                             .SetTooltip("The most lines Up and Down walk through, saved between runs."),
                         OPAAX_PROP(LogLevel).SetTooltip("The lowest level of the log lines shown."),
                         OPAAX_PROP(BackgroundColor),
                         OPAAX_PROP(TextColor),
                         OPAAX_PROP(InputColor),
                         OPAAX_PROP(WarningColor),
                         OPAAX_PROP(ErrorColor))
    };
}
