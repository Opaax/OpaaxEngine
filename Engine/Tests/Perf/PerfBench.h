// PerfBench.h — small micro-benchmark helper for the "perf" suite.
//   Measure() runs the caller's lambda (InOps operations) once per epoch over InEpochs epochs and
//   returns the median ns per op (robust to outliers), after one warm-up call.
//   Run in Release (build.bat bench): Debug numbers are meaningless.
#pragma once

#include "Core/Algo/Algo.h"
#include "Core/Time/Clock.h"
#include "Core/OpaaxTypes.h"
#include "Core/IO/Console.h"

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
    Result Measure(Uint64 InOps, int InEpochs, TFn&& InFn)
    {
        InFn(); // warmup — not timed

        TDynArray<double> lNsPerOp;
        lNsPerOp.reserve(static_cast<size_t>(InEpochs));

        for (int lEpoch = 0; lEpoch < InEpochs; ++lEpoch)
        {
            const Uint64 lStart = Clock::NowNanoseconds();
            InFn();
            const Uint64 lEnd = Clock::NowNanoseconds();

            const double lNs = static_cast<double>(lEnd - lStart);
            lNsPerOp.push_back(lNs / static_cast<double>(InOps));
        }

        Algo::Sort(lNsPerOp);
        const double lMedian = lNsPerOp[lNsPerOp.size() / 2];

        return { lMedian, lMedian > 0.0 ? 1000.0 / lMedian : 0.0 }; // ns/op -> Mops/s
    }

    // Print the number (ALWAYS — watch the printed ns/op yourself for smaller ~2x drift) and
    // soft-gate it. InBudgetNsPerOp is a GENEROUS "catch a catastrophe" bound (an O(n^2) or a
    // per-op heap allocation creeping in), deliberately loose so it never flakes on slower CI /
    // hardware. It is NOT a tight bound.
    inline void ReportAndGate(const char* InLabel, const Result& InResult, double InBudgetNsPerOp)
    {
        Console::WriteLine(OpaaxString::Format("  [perf] {:<38} {:9.1f} ns/op {:8.1f} Mops/s  (budget {:.0f} ns/op)",
                                               InLabel, InResult.MedianNsPerOp, InResult.MOpsPerSec, InBudgetNsPerOp));

        const OpaaxString lMsg = OpaaxString::Format("{}: {:.1f} ns/op exceeded budget {:.0f} ns/op",
                                                     InLabel, InResult.MedianNsPerOp, InBudgetNsPerOp);
        CHECK_MESSAGE(InResult.MedianNsPerOp <= InBudgetNsPerOp, lMsg);
    }
}
