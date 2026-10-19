// Suite: Logger, on local instances. The global one is never initialized here (keeps the suite silent).
#include <doctest.h>

#include "CaptureSink.h"
#include "Core/Log/Logger.h"

using namespace Opaax;

namespace
{
    OPAAX_LOG_CATEGORY(LoggerTest);

    // spdlog ends a line with the platform's EOL (\r\n on Windows).
    OpaaxString Line(const char* InText)
    {
        return OpaaxString(InText) + spdlog::details::os::default_eol;
    }
}

TEST_CASE("Logger: a line is '[Category] message', formatted from the call site's arguments")
{
    Logger                        lLogger;
    const TSharedPtr<CaptureSink> lOut = CaptureSink::Make();
    lLogger.AttachSink(lOut);

    lLogger.Logf(ELogLevel::Info, LogLoggerTest, "{} + {} = {}", 1, 2, 3);
    lLogger.Log(ELogLevel::Warn, LogLoggerTest, OpaaxStringView("braces {} are text here"));

    CHECK(lOut->GetText() == Line("[LoggerTest] 1 + 2 = 3") + Line("[LoggerTest] braces {} are text here"));
}

TEST_CASE("Logger: lines before any sink are held, then replayed in order on attach")
{
    Logger lLogger;
    CHECK_FALSE(lLogger.HasSinks());

    lLogger.Logf(ELogLevel::Info, LogLoggerTest, "first");
    lLogger.Logf(ELogLevel::Error, LogLoggerTest, "second");
    CHECK(lLogger.GetPendingCount() == 2);

    const TSharedPtr<CaptureSink> lOut = CaptureSink::Make();
    lLogger.AttachSink(lOut);

    CHECK(lLogger.HasSinks());
    CHECK(lLogger.GetPendingCount() == 0);
    CHECK(lOut->GetText() == Line("[LoggerTest] first") + Line("[LoggerTest] second"));
}

TEST_CASE("Logger: held lines reach EVERY sink attached together (console AND file at Init)")
{
    Logger lLogger;
    lLogger.Logf(ELogLevel::Info, LogLoggerTest, "early");

    const TSharedPtr<CaptureSink> lConsole = CaptureSink::Make();
    const TSharedPtr<CaptureSink> lFile    = CaptureSink::Make();
    lLogger.AttachSinks({ lConsole, lFile });

    CHECK(lConsole->GetText() == Line("[LoggerTest] early"));
    CHECK(lFile->GetText()    == Line("[LoggerTest] early"));
}

TEST_CASE("Logger: the held lines are bounded, keep the FIRST ones and report the rest")
{
    Logger lLogger;

    for (Uint32 i = 0; i < Logger::MAX_PENDING_LINES + 3; ++i)
    {
        lLogger.Logf(ELogLevel::Trace, LogLoggerTest, "line {}", i);
    }

    CHECK(lLogger.GetPendingCount() == Logger::MAX_PENDING_LINES);

    const TSharedPtr<CaptureSink> lOut = CaptureSink::Make();
    lLogger.AttachSink(lOut);

    const OpaaxString& lText = lOut->GetText();
    CHECK(lText.StartsWith(Line("[LoggerTest] line 0")));
    CHECK(lText.Contains(Line("line 255")));
    CHECK_FALSE(lText.Contains(Line("line 256")));
    CHECK(lText.Contains("3 line(s) logged before the sinks existed were dropped"));
}

TEST_CASE("Logger: after Shutdown nothing reaches the old sink and logging stays safe")
{
    Logger                        lLogger;
    const TSharedPtr<CaptureSink> lOut = CaptureSink::Make();
    lLogger.AttachSink(lOut);

    lLogger.Logf(ELogLevel::Info, LogLoggerTest, "before");
    lLogger.Shutdown();
    lLogger.Logf(ELogLevel::Info, LogLoggerTest, "after");

    CHECK_FALSE(lLogger.HasSinks());
    CHECK(lOut->GetText() == Line("[LoggerTest] before"));
    CHECK(lLogger.GetPendingCount() == 1);
}

TEST_CASE("Logger: Get() is one instance")
{
    CHECK(&Logger::Get() == &Logger::Get());
}
