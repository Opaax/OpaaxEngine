// PerfBench.h — small micro-benchmark helper for the "perf" suite.
//   Measure() runs the caller's lambda (InOps operations) once per epoch over InEpochs epochs and
//   returns the median ns per op (robust to outliers), after one warm-up call.
//   Run in Release (build.bat bench): Debug numbers are meaningless.
#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

#include <doctest.h>

namespace Opaax::Perf
{
    struct Result
    {
        double MedianNsPerOp = 0.0;
        double MOpsPerSec    = 0.0;
    };

    // InOps: how many operations the lambda performs per call. InEpochs: timed repetitions
    // (use an ODD count so the median is a single sample). Returns the median ns-per-op.
    template<typename TFn>
    Result Measure(std::uint64_t InOps, int InEpochs, TFn&& InFn)
    {
        using Clock = std::chrono::steady_clock;

        InFn(); // warmup — not timed

        std::vector<double> lNsPerOp;
        lNsPerOp.reserve(static_cast<std::size_t>(InEpochs));

        for (int lEpoch = 0; lEpoch < InEpochs; ++lEpoch)
        {
            const auto lStart = Clock::now();
            InFn();
            const auto lEnd = Clock::now();

            const double lNs = std::chrono::duration<double, std::nano>(lEnd - lStart).count();
            lNsPerOp.push_back(lNs / static_cast<double>(InOps));
        }

        std::sort(lNsPerOp.begin(), lNsPerOp.end());
        const double lMedian = lNsPerOp[lNsPerOp.size() / 2];

        return { lMedian, lMedian > 0.0 ? 1000.0 / lMedian : 0.0 }; // ns/op -> Mops/s
    }

    // Print the number (ALWAYS — watch the printed ns/op yourself for smaller ~2x drift) and
    // soft-gate it. InBudgetNsPerOp is a GENEROUS "catch a catastrophe" bound (an O(n^2) or a
    // per-op heap allocation creeping in), deliberately loose so it never flakes on slower CI /
    // hardware. It is NOT a tight bound.
    inline void ReportAndGate(const char* InLabel, const Result& InResult, double InBudgetNsPerOp)
    {
        std::printf("  [perf] %-38s %9.1f ns/op %8.1f Mops/s  (budget %.0f ns/op)\n",
                    InLabel, InResult.MedianNsPerOp, InResult.MOpsPerSec, InBudgetNsPerOp);
        std::fflush(stdout);

        char lMsg[192];
        std::snprintf(lMsg, sizeof(lMsg), "%s: %.1f ns/op exceeded budget %.0f ns/op",
                      InLabel, InResult.MedianNsPerOp, InBudgetNsPerOp);
        CHECK_MESSAGE(InResult.MedianNsPerOp <= InBudgetNsPerOp, lMsg);
    }
}
