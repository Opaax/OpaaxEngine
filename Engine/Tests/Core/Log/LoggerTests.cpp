// Suite: Logger — the I1 singleton's class, tested on OWN instances (SG4). The global is never
// initialised by the test runner, which is what keeps the suite silent.
#include <doctest.h>

#include "Core/Log/Logger.h"

#include <spdlog/sinks/ostream_sink.h>

#include <sstream>

using namespace Opaax;

namespace
{
    OPAAX_LOG_CATEGORY(LoggerTest);

    // Message text only, one per line, so a test asserts exactly what the call site produced.
    spdlog::sink_ptr MakeCaptureSink(std::ostringstream& OutStream)
    {
        spdlog::sink_ptr lSink = std::make_shared<spdlog::sinks::ostream_sink_mt>(OutStream);
        lSink->set_pattern("%v");
        return lSink;
    }

    // spdlog ends a line with the platform's EOL (\r\n on Windows).
    std::string Line(const char* InText)
    {
        return std::string(InText) + spdlog::details::os::default_eol;
    }
}

TEST_CASE("Logger: a line is '[Category] message', formatted from the call site's arguments")
{
    Logger             lLogger;
    std::ostringstream lOut;
    lLogger.AttachSink(MakeCaptureSink(lOut));

    lLogger.Logf(ELogLevel::Info, LogLoggerTest, "{} + {} = {}", 1, 2, 3);
    lLogger.Log(ELogLevel::Warn, LogLoggerTest, OpaaxStringView("braces {} are text here"));

    CHECK(lOut.str() == Line("[LoggerTest] 1 + 2 = 3") + Line("[LoggerTest] braces {} are text here"));
}

TEST_CASE("Logger: lines before any sink are held, then replayed in order on attach")
{
    Logger lLogger;
    CHECK_FALSE(lLogger.HasSinks());

    lLogger.Logf(ELogLevel::Info, LogLoggerTest, "first");
    lLogger.Logf(ELogLevel::Error, LogLoggerTest, "second");
    CHECK(lLogger.GetPendingCount() == 2);

    std::ostringstream lOut;
    lLogger.AttachSink(MakeCaptureSink(lOut));

    CHECK(lLogger.HasSinks());
    CHECK(lLogger.GetPendingCount() == 0);
    CHECK(lOut.str() == Line("[LoggerTest] first") + Line("[LoggerTest] second"));
}

TEST_CASE("Logger: held lines reach EVERY sink attached together (console AND file at Init)")
{
    Logger lLogger;
    lLogger.Logf(ELogLevel::Info, LogLoggerTest, "early");

    std::ostringstream lConsole;
    std::ostringstream lFile;
    lLogger.AttachSinks({ MakeCaptureSink(lConsole), MakeCaptureSink(lFile) });

    CHECK(lConsole.str() == Line("[LoggerTest] early"));
    CHECK(lFile.str()    == Line("[LoggerTest] early"));
}

TEST_CASE("Logger: the held lines are bounded, keep the FIRST ones and report the rest")
{
    Logger lLogger;

    for (Uint32 i = 0; i < Logger::MAX_PENDING_LINES + 3; ++i)
    {
        lLogger.Logf(ELogLevel::Trace, LogLoggerTest, "line {}", i);
    }

    CHECK(lLogger.GetPendingCount() == Logger::MAX_PENDING_LINES);

    std::ostringstream lOut;
    lLogger.AttachSink(MakeCaptureSink(lOut));

    const std::string lText = lOut.str();
    CHECK(lText.rfind(Line("[LoggerTest] line 0"), 0) == 0);
    CHECK(lText.find(Line("line 255")) != std::string::npos);
    CHECK(lText.find(Line("line 256")) == std::string::npos);
    CHECK(lText.find("3 line(s) logged before the sinks existed were dropped") != std::string::npos);
}

TEST_CASE("Logger: after Shutdown nothing reaches the old sink and logging stays safe")
{
    Logger             lLogger;
    std::ostringstream lOut;
    lLogger.AttachSink(MakeCaptureSink(lOut));

    lLogger.Logf(ELogLevel::Info, LogLoggerTest, "before");
    lLogger.Shutdown();
    lLogger.Logf(ELogLevel::Info, LogLoggerTest, "after");

    CHECK_FALSE(lLogger.HasSinks());
    CHECK(lOut.str() == Line("[LoggerTest] before"));
    CHECK(lLogger.GetPendingCount() == 1);
}

TEST_CASE("Logger: Get() is one instance")
{
    CHECK(&Logger::Get() == &Logger::Get());
}
