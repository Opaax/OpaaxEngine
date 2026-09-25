// Suite: the Log panel's filter (Editor/Panels/LogFilter.h) — block LG2.
//
// What a line needs to be shown: its level's button on, and the search found in its message,
// ignoring case. Critical rides Error's button.
#include <doctest.h>

#include "Editor/Panels/LogFilter.h"

using namespace Opaax;
using namespace Opaax::Editor;

namespace
{
    LogEntry MakeEntry(const ELogLevel InLevel, const char* InMessage)
    {
        LogEntry lEntry;
        lEntry.Level   = InLevel;
        lEntry.Message = OpaaxString(InMessage);
        return lEntry;
    }
}

TEST_CASE("LogFilter: ContainsNoCase is an ASCII case-insensitive substring")
{
    CHECK(ContainsNoCase("World 'Main' loaded", "world"));
    CHECK(ContainsNoCase("World 'Main' loaded", "MAIN' LO"));
    CHECK(ContainsNoCase("anything", ""));
    CHECK(ContainsNoCase("", ""));
    CHECK_FALSE(ContainsNoCase("World", "worlds"));
    CHECK_FALSE(ContainsNoCase("abc", "abd"));
    CHECK(ContainsNoCase("aab", "ab"));   // a false start one char in must not hide the match
}

TEST_CASE("LogFilter: default shows every line")
{
    const LogFilter lFilter;
    for (const ELogLevel lLevel : { ELogLevel::Trace, ELogLevel::Info, ELogLevel::Warn, ELogLevel::Error, ELogLevel::Critical })
    {
        CHECK(lFilter.Passes(MakeEntry(lLevel, "x")));
    }
}

TEST_CASE("LogFilter: a level button off hides its lines; Critical follows Error")
{
    LogFilter lFilter;
    lFilter.ShowLevel[static_cast<size_t>(ELogLevelFilter::Info)]  = false;
    lFilter.ShowLevel[static_cast<size_t>(ELogLevelFilter::Error)] = false;

    CHECK_FALSE(lFilter.Passes(MakeEntry(ELogLevel::Info, "x")));
    CHECK_FALSE(lFilter.Passes(MakeEntry(ELogLevel::Error, "x")));
    CHECK_FALSE(lFilter.Passes(MakeEntry(ELogLevel::Critical, "x")));
    CHECK(lFilter.Passes(MakeEntry(ELogLevel::Warn, "x")));
    CHECK(lFilter.Passes(MakeEntry(ELogLevel::Trace, "x")));
}

TEST_CASE("LogFilter: the search AND the level must both pass")
{
    LogFilter lFilter;
    lFilter.Search = "world";

    CHECK(lFilter.Passes(MakeEntry(ELogLevel::Info, "Active World changed")));
    CHECK_FALSE(lFilter.Passes(MakeEntry(ELogLevel::Info, "Panel built")));

    lFilter.ShowLevel[static_cast<size_t>(ELogLevelFilter::Info)] = false;
    CHECK_FALSE(lFilter.Passes(MakeEntry(ELogLevel::Info, "Active World changed")));
}
