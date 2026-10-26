// TestManualJobs.h — a job system that runs nothing until RunAll: what it was given stays queued, so a
//   test sees a background load before and after it finishes.
#pragma once

#include "Application/Services/IJobSystem.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    class TestManualJobs final : public IJobSystem
    {
    public:
        JobHandle Submit(TFunction<void()> InWork) override { return Submit(Move(InWork), {}); }

        JobHandle Submit(TFunction<void()> InWork, TFunction<void()> InOnComplete) override
        {
            m_Jobs.emplace_back(Move(InWork), Move(InOnComplete));
            return JobHandle{};
        }

        void ParallelFor(const Uint32 InCount, const TFunction<void(Uint32)>& InBody, Uint32) override
        {
            for (Uint32 lIndex = 0; lIndex < InCount; ++lIndex) { InBody(lIndex); }
        }

        void   Wait(const JobHandle&) override {}
        void   DrainCompletions() override {}
        Uint32 GetWorkerCount() const noexcept override { return 0; }

        /** Runs every queued job, then its completion. */
        void RunAll()
        {
            auto lJobs = Move(m_Jobs);
            m_Jobs.clear();
            for (auto& [lWork, lDone] : lJobs)
            {
                if (lWork) { lWork(); }
                if (lDone) { lDone(); }
            }
        }

        bool HasJobs() const noexcept { return !m_Jobs.empty(); }

    private:
        TDynArray<TPair<TFunction<void()>, TFunction<void()>>> m_Jobs;
    };
}
