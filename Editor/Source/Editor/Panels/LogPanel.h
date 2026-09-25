#pragma once

#include "Core/Log/LogHistory.h"
#include "Editor/Panels/IEditorPanel.h"

#include <deque>

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // LogPanel — the log, inside the editor (block LG). Reads Logger::CopyHistorySince and nothing
    //   else, so it shows exactly the lines the console and the file got, boot lines included (the
    //   editor turns the history on before Bootstrap).
    //
    //   It keeps its OWN copy: the Logger's lock is held for the copy of the new lines, never while
    //   drawing, and Clear empties this copy without touching what the engine holds.
    // =============================================================================
    class LogPanel final : public IEditorPanel
    {
        // =============================================================================
        // Statics
        // =============================================================================
    public:
        OPAAX_EDITOR_PANEL_NAME(Log);

        /** What the panel shows at most — and what the editor asks the Logger to keep. */
        static constexpr Uint32 MAX_LINES = 4096;

        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        explicit LogPanel(EditorContext& InContext);
        ~LogPanel() override;

        // =============================================================================
        // Copy delete
        // =============================================================================
        LogPanel(const LogPanel&)            = delete;
        LogPanel& operator=(const LogPanel&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /** Appends the lines logged since the last frame, oldest dropped past MAX_LINES. */
        void PullNewLines();

        /** Time | Level | Message, clipped to the visible rows, following the bottom while it is there. */
        void DrawLines();

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        void Startup()     override {}
        void OnPreRender() override {}
        void DrawContents() override;
        void Shutdown()    override {}

        PanelWindowStyle GetWindowStyle() const override { return { { 720.f, 260.f } }; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        std::deque<LogEntry> m_Entries;

        /** Reused every frame for the copy out of the Logger, so pulling allocates nothing when idle. */
        TDynArray<LogEntry>  m_Incoming;

        Uint64               m_LastSequence = 0;
    };
}
