// CaptureSink.h — a log sink that keeps the lines it receives, for the tests to read back.
#pragma once

#include <spdlog/sinks/base_sink.h>

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    class CaptureSink final : public spdlog::sinks::base_sink<Mutex>
    {
    public:
        /** A sink keeping the message text only, one per line: what the call site produced. */
        static TSharedPtr<CaptureSink> Make()
        {
            TSharedPtr<CaptureSink> lSink = MakeShared<CaptureSink>();
            lSink->set_pattern("%v");
            return lSink;
        }

        /** Every line received, each ended by the platform's end of line (spdlog's). */
        const OpaaxString& GetText() const noexcept { return m_Text; }

    protected:
        void sink_it_(const spdlog::details::log_msg& InMessage) override
        {
            spdlog::memory_buf_t lLine;
            formatter_->format(InMessage, lLine);
            m_Text.Append(lLine.data(), static_cast<Uint32>(lLine.size()));
        }

        void flush_() override {}

    private:
        OpaaxString m_Text;
    };
}
