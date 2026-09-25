#pragma once

#include "Core/Log/LogHistory.h"
#include "Editor/Panels/IEditorPanel.h"
#include "Editor/Panels/LogFilter.h"

#include <array>
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
    //
    //   FILTERING IS INCREMENTAL. m_Shown lists the sequences that pass the filter; a new line is
    //   tested once, on arrival, and only a change to the filter walks every line again. That is
    //   why m_Entries is kept CONTIGUOUS in sequence — a shown sequence finds its line by
    //   subtraction, not by a search.
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

        /** Clear, the level buttons, the search. Refilters when any of them changed. */
        void DrawToolbar();

        /** One level's button, "Warn 3". @return true when it was clicked. */
        bool DrawLevelToggle(ELogLevelFilter InLevel);

        /** Time | Level | Message, clipped to the visible rows, following the bottom while it is there. */
        void DrawLines();

        /** Rebuilds m_Shown from every held line — the filter changed. */
        void Refilter();

        /** Empties the panel's copy (never the Logger's). */
        void ClearLines();

        const LogEntry& EntryAt(Uint64 InSequence) const;

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
        static constexpr size_t LEVEL_COUNT = static_cast<size_t>(ELogLevelFilter::Count);

        /** Contiguous in sequence (see the header comment). */
        std::deque<LogEntry> m_Entries;

        /** Sequences of the lines in m_Entries that pass m_Filter, oldest first. */
        std::deque<Uint64>   m_Shown;

        /** Lines per level button in m_Entries — ALL of them, whatever the filter hides. */
        std::array<Uint32, LEVEL_COUNT> m_LevelCounts{};

        LogFilter            m_Filter;

        /** ImGui edits this; m_Filter.Search is its copy, taken when it changes. */
        char                 m_SearchBuffer[128] = {};

        /** Reused every frame for the copy out of the Logger, so pulling allocates nothing when idle. */
        TDynArray<LogEntry>  m_Incoming;

        Uint64               m_LastSequence = 0;
    };
}
