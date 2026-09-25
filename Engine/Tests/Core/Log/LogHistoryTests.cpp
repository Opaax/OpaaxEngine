// Suite: LogHistory — the Logger's structured tail, read by the editor's Log panel (block LG).
#include <doctest.h>

#include "Core/Log/LogHistory.h"
#include "Core/Log/Logger.h"

#include <spdlog/sinks/ostream_sink.h>

#include <sstream>

using namespace Opaax;

namespace
{
    OPAAX_LOG_CATEGORY(HistoryTest);

    TDynArray<LogEntry> CopyAll(const LogHistory& InHistory)
    {
        TDynArray<LogEntry> lEntries;
        InHistory.CopySince(0, lEntries);
        return lEntries;
    }
}

TEST_CASE("LogHistory: capacity 0 is off — nothing is kept, no sequence is spent")
{
    LogHistory lHistory;
    CHECK_FALSE(lHistory.IsEnabled());

    lHistory.Push(ELogLevel::Info, OPAAX_ID("Cat"), "dropped");

    TDynArray<LogEntry> lEntries;
    CHECK(lHistory.CopySince(0, lEntries) == 0);
    CHECK(lEntries.empty());
}

TEST_CASE("LogHistory: CopySince returns only the lines after the reader's last sequence")
{
    LogHistory lHistory(8);
    lHistory.Push(ELogLevel::Info, OPAAX_ID("Cat"), "one");
    lHistory.Push(ELogLevel::Warn, OPAAX_ID("Cat"), "two");

    TDynArray<LogEntry> lFirst;
    const Uint64 lSeen = lHistory.CopySince(0, lFirst);
    REQUIRE(lFirst.size() == 2);
    CHECK(lSeen == 2);
    CHECK(lFirst[0].Message == OpaaxString("one"));
    CHECK(lFirst[1].Level == ELogLevel::Warn);

    lHistory.Push(ELogLevel::Error, OPAAX_ID("Other"), "three");

    TDynArray<LogEntry> lSecond;
    CHECK(lHistory.CopySince(lSeen, lSecond) == 3);
    REQUIRE(lSecond.size() == 1);
    CHECK(lSecond[0].Sequence == 3);
    CHECK(lSecond[0].Category == OPAAX_ID("Other"));

    TDynArray<LogEntry> lNothing;
    CHECK(lHistory.CopySince(3, lNothing) == 3);
    CHECK(lNothing.empty());
}

TEST_CASE("LogHistory: full, the OLDEST line goes; a reader that fell behind gets what is still held")
{
    LogHistory lHistory(3);
    for (const char* lText : { "a", "b", "c", "d", "e" })
    {
        lHistory.Push(ELogLevel::Info, OPAAX_ID("Cat"), lText);
    }

    const TDynArray<LogEntry> lEntries = CopyAll(lHistory);
    REQUIRE(lEntries.size() == 3);
    CHECK(lEntries[0].Message == OpaaxString("c"));
    CHECK(lEntries[0].Sequence == 3);
    CHECK(lEntries[2].Message == OpaaxString("e"));

    // Seen up to 1; line 2 is gone, so it resumes at the oldest held.
    TDynArray<LogEntry> lBehind;
    lHistory.CopySince(1, lBehind);
    CHECK(lBehind.size() == 3);

    lHistory.SetCapacity(1);
    REQUIRE(lHistory.GetCount() == 1);
    CHECK(CopyAll(lHistory)[0].Message == OpaaxString("e"));
}

TEST_CASE("Logger: history is off by default, and on it keeps the message WITHOUT the category prefix")
{
    Logger              lLogger;
    TDynArray<LogEntry> lEntries;

    lLogger.Logf(ELogLevel::Info, LogHistoryTest, "not kept");
    CHECK(lLogger.CopyHistorySince(0, lEntries) == 0);

    lLogger.EnableHistory(16);

    // Before any sink: held for Init AND kept in the history.
    lLogger.Logf(ELogLevel::Warn, LogHistoryTest, "{} apples", 3);
    CHECK(lLogger.GetPendingCount() == 2);

    // Two sinks, attached together (L102) — the sink text is the same "[Category] message" it always
    // was, held lines included, whether or not the history kept them.
    std::ostringstream          lOutA;
    std::ostringstream          lOutB;
    TDynArray<spdlog::sink_ptr> lSinks;
    for (std::ostringstream* lOut : { &lOutA, &lOutB })
    {
        lSinks.push_back(std::make_shared<spdlog::sinks::ostream_sink_mt>(*lOut));
        lSinks.back()->set_pattern("%v");
    }
    lLogger.AttachSinks(lSinks);
    lLogger.Logf(ELogLevel::Error, LogHistoryTest, "after");

    CHECK(lLogger.CopyHistorySince(0, lEntries) == 2);
    REQUIRE(lEntries.size() == 2);
    CHECK(lEntries[0].Message == OpaaxString("3 apples"));
    CHECK(lEntries[0].Level == ELogLevel::Warn);
    CHECK(lEntries[0].Category == OPAAX_ID("HistoryTest"));
    CHECK(lEntries[1].Message == OpaaxString("after"));

    const std::string lEol = spdlog::details::os::default_eol;
    const std::string lExpected = "[HistoryTest] not kept" + lEol + "[HistoryTest] 3 apples" + lEol
                                + "[HistoryTest] after" + lEol;
    CHECK(lOutA.str() == lExpected);
    CHECK(lOutB.str() == lExpected);
}
