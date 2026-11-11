#pragma once

#include <nlohmann/json.hpp>

#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    /** What a console row shows: its colour. */
    enum class EDevConsoleRow : Uint8
    {
        Input,     // a line typed
        Answer,    // what a command answered
        Info,      // help, a list of matches, a log line
        Warning,
        Error
    };

    /** The name of a row kind ("error"), as console.info answers it. */
    const char* ToString(EDevConsoleRow InKind) noexcept;

    /** The log level a row of InKind is logged at (the editor's Log panel): an error is a warning, a typing mistake. */
    ELogLevel ToLogLevel(EDevConsoleRow InKind) noexcept;

    /**
     * What a command's answer prints: nothing for null or an empty object (the command only did
     * something), a string as it is, anything else as JSON indented by InIndent.
     */
    OpaaxString FormatCommandAnswer(const nlohmann::json& InValue, int InIndent = 2);

    /** One row of the console's output. */
    struct DevConsoleRow
    {
        EDevConsoleRow Kind = EDevConsoleRow::Info;
        OpaaxString    Text;
    };

    // =============================================================================
    // DevConsoleOutput — the console's rows, oldest first, the oldest dropped past the cap. A text of
    //   several lines is a row each, and an answer is its JSON indented over several rows: rows are
    //   not wrapped. GetRevision changes with every change, so a view rebuilds only then. Pure.
    // =============================================================================
    class DevConsoleOutput
    {
    public:
        static constexpr Uint32 DEFAULT_MAX_ROWS = 200;

        /** Spaces a JSON answer is indented by. */
        static constexpr int JSON_INDENT = 2;

        explicit DevConsoleOutput(Uint32 InMaxRows = DEFAULT_MAX_ROWS);

        /** The most rows kept (at least 1). */
        void   SetMaxRows(Uint32 InMaxRows);
        Uint32 GetMaxRows() const noexcept { return m_MaxRows; }

        /** Adds InText, a row per line. Tabs become spaces; an empty text adds an empty row. */
        void Add(EDevConsoleRow InKind, const OpaaxString& InText);

        /**
         * Adds a command's answer: nothing for an empty object (the command only did something), a
         * string as text, anything else as indented JSON.
         */
        void AddAnswer(const nlohmann::json& InValue);

        void Clear();

        const TDeque<DevConsoleRow>& GetRows() const noexcept { return m_Rows; }

        /** Bumped by every change. */
        Uint64 GetRevision() const noexcept { return m_Revision; }

    private:
        void Push(EDevConsoleRow InKind, OpaaxString InText);

        TDeque<DevConsoleRow> m_Rows;
        Uint32                m_MaxRows;
        Uint64                m_Revision = 0;
    };
}
