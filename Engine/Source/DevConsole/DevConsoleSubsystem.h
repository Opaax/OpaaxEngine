#pragma once

#include "Commands/CommandHandle.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "DevConsole/DevConsoleSettings.h"
#include "Engine/GameInstance/GameInstanceSubsystem.h"

namespace Opaax
{
    class DevConsole;
    class InputMappingSubsystem;
    class UICanvas;
    class UIText;

    // =============================================================================
    // DevConsoleSubsystem — the dev console of a game: the toggle key (Engine.config's Console
    //   section) opens it over the top of the screen, on its own canvas drawn above the game's UI.
    //   While open it holds the game's input (InputMappingSubsystem::HoldInput): no action fires and
    //   the UI gets nothing, though code that reads raw keys still sees them. Shows the log lines from
    //   the configured level. The history is kept in <UserDir>/Console/History.txt. In a ship build
    //   (a registry that refuses DevOnly commands) it makes nothing.
    // =============================================================================
    class DevConsoleSubsystem final : public GameInstanceSubsystem
    {
        OPAAX_OBJECT(DevConsoleSubsystem, GameInstanceSubsystem)

    public:
        DevConsoleSubsystem();
        ~DevConsoleSubsystem() override;

        /** The history file, under the user's folder. */
        static constexpr const char* HISTORY_FILE = "Console/History.txt";

        /** The face the console is written with. */
        static constexpr const char* FONT = "/Engine/Fonts/Roboto/roboto-latin-400-normal.ttf";

        /** The canvas's height in its own units: the font size is in pixels of a 1080-pixel screen. */
        static constexpr float REFERENCE_HEIGHT = 1080.f;

        /** The log lines the console keeps at least (Logger::EnsureHistory). */
        static constexpr Uint32 LOG_HISTORY = 1000;

        /** The console, or null in a ship build. */
        DevConsole* GetConsole() const noexcept { return m_Console.get(); }

        //~Begin Subsystem interface
        /** Before the UI (100) and the input mapping (200): it takes the keys first. */
        Int32 GetTickOrder() const override { return 50; }

        bool Startup() override;
        void Update(double InDeltaTime) override;
        void Shutdown() override;
        //~End Subsystem interface

    private:
        void BuildCanvas();

        /** Writes the rows and the line again when they changed. */
        void RefreshCanvas();

        /** Adds the log lines logged since the last frame, from the configured level. */
        void PullLog();

        /** The console opened or closed: the input held or let go, the world paused if configured. */
        void OnOpenChanged(bool bInOpen);

        void LoadHistory();
        void SaveHistory();

        /** Shared: a line that ends the game shuts the subsystem down inside the console's Tick. */
        TSharedPtr<DevConsole>   m_Console;
        TDynArray<CommandHandle> m_Commands;
        ConsoleSettings          m_Settings;
        OpaaxString              m_HistoryPath;

        InputMappingSubsystem* m_Mapping = nullptr;   // a sibling; may be null

        TUniquePtr<UICanvas> m_Canvas;
        TDynArray<UIText*>   m_Rows;               // owned by the canvas, the newest first
        UIText*              m_LineText = nullptr;

        Uint64      m_ShownRevision = 0;
        Uint32      m_ShownScroll   = 0;
        OpaaxString m_ShownLine;
        bool        m_bShownAny     = false;

        Uint64 m_LogSequence  = 0;
        bool   m_bWasOpen     = false;
        bool   m_bPausedWorld = false;
    };
}
