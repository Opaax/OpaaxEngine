// PerfBench.h — tiny statistical micro-bench helper for the "perf" suite.
//
// WHY homegrown (not nanobench / Catch2-bench): a LOOSE regression gate only needs a stable
// central-tendency number, and median-of-epochs gives exactly that. ~70 lines we fully own beat
// a 3k-line vendored header (engine value #1: simple, understandable end-to-end). If you ever need
// instruction-counter rigor, swap the body of Measure() — the call sites don't change.
//
// Model: the caller's lambda performs InOps operations; Measure() times the whole lambda once per
// epoch across InEpochs epochs and returns the MEDIAN ns-per-op (outlier-robust — a GC pause / OS
// hiccup in one epoch can't move the median). One warmup call pages memory in and primes caches
// before timing begins.
//
// IMPORTANT: run this in the RELEASE OpaaxTests build (`build.bat bench`). Debug numbers are
// dominated by iterator-debugging / no-inline overhead and are meaningless for budgets.
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
