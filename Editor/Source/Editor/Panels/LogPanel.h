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
    // LogPanel — the log inside the editor. Reads Logger::CopyHistorySince, so it shows the same
    //   lines as the console and the file, boot lines included.
    //   Keeps its own copy (Clear only empties it). Filtering is incremental: new lines are tested
    //   once; only a filter change re-tests every line. m_Entries is contiguous in sequence.
    // =============================================================================
    class LogPanel final : public IEditorPanel
    {
        // =============================================================================
        // Statics
        // =============================================================================
    public:
        OPAAX_EDITOR_PANEL_NAME(Log);

        /** Maximum lines shown (also what the editor asks the Logger to keep). */
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
        /** Appends the lines logged since last frame, dropping the oldest past MAX_LINES. */
        void PullNewLines();

        /** Clear, level buttons, categories, search. Refilters when one of them changed. */
        void DrawToolbar();

        /** The Categories dropdown: All / None, then one checkbox per category. @return True if changed */
        bool DrawCategoryFilter();

        /** Adds InCategory to m_Categories (sorted by name) the first time it is seen. */
        void NoteCategory(OpaaxStringID InCategory);

        /** One level's button, "Warn 3". @return True if clicked */
        bool DrawLevelToggle(ELogLevelFilter InLevel);

        /** Time | Level | Category | Message, clipped to the visible rows, following the bottom when there. */
        void DrawLines();

        /** Rebuilds m_Shown from every line (the filter changed). */
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

        /** Contiguous in sequence. */
        std::deque<LogEntry> m_Entries;

        /** Sequences of the lines that pass m_Filter, oldest first. */
        std::deque<Uint64>   m_Shown;

        /** Lines per level button (all of them, whatever the filter). */
        std::array<Uint32, LEVEL_COUNT> m_LevelCounts{};

        LogFilter            m_Filter;

        /**
         * Every category seen, sorted by name (the dropdown's rows). Kept by Clear.
         */
        TDynArray<OpaaxStringID> m_Categories;

        /** ImGui edits this; m_Filter.Search is its copy. */
        char                 m_SearchBuffer[128] = {};

        /** Reused every frame, so copying from the Logger allocates nothing when idle. */
        TDynArray<LogEntry>  m_Incoming;

        Uint64               m_LastSequence = 0;
    };
}
