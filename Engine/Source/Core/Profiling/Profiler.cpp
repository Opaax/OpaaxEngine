#include "Profiler.h"

#include <chrono>

namespace Opaax
{
    namespace
    {
        double NowSeconds()
        {
            using namespace std::chrono;
            return duration<double>(steady_clock::now().time_since_epoch()).count();
        }
    }

    Profiler& Profiler::Get()
    {
        // Leaked on purpose (SG5).
        static Profiler* s_Instance = new Profiler();
        return *s_Instance;
    }

    Profiler::Profiler()
        : m_RecordingThread(std::this_thread::get_id())
    {
    }

    Profiler::~Profiler() = default;

    void Profiler::Init(const bool bInEnabled)
    {
        m_RecordingThread = std::this_thread::get_id();
        m_bEnabled        = bInEnabled;
    }

    void Profiler::Shutdown()
    {
        m_bEnabled = false;
    }

    void Profiler::BeginFrame()
    {
        if (!m_bEnabled) { return; }

        const double lNow = NowSeconds();

        // The frame that just ended becomes readable, whole — including its Present, which happens
        // after the engine's tick.
        m_Stats.Profiler.Publish();

        // The first call has no previous frame; the time since process start would put one absurd
        // sample at the front of every graph.
        m_Stats.FrameMs = m_LastFrameStart > 0.0 ? (lNow - m_LastFrameStart) * 1000.0 : 0.0;

        m_Stats.GpuMs    = m_RecordingGpuMs;
        m_RecordingGpuMs = -1.0;

        m_LastFrameStart = lNow;
    }

    void Profiler::SubmitGpuMs(const double InGpuMs)
    {
        if (!m_bEnabled) { return; }

        m_RecordingGpuMs = InGpuMs;
    }

    void Profiler::AddCount(const char* InName, const Uint64 InValue)
    {
        if (FrameProfiler* lRecorder = GetRecorder())
        {
            lRecorder->AddCount(InName, InValue);
        }
    }
}
