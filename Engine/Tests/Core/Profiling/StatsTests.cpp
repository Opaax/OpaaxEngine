// Suite: the frame profiler + the sample ring (Core/Profiling/, ④).
//
// Both are pure and GPU-free, which is the whole reason the logic lives in them rather than in the
// panel: the tree's SHAPE (pre-order, correct depths, a complete previous frame) and the ring's
// wrap-around are exactly what a smoke run cannot check by eye.
#include <doctest.h>

#include "Core/Profiling/FrameProfiler.h"
#include "Core/Profiling/StatsHistory.h"
#include "Editor/Panels/StatsDisplay.h"   // header-only, no ImGui — the M2a include path (L55)

using namespace Opaax;

namespace
{
    /** Record one closed scope at the current depth. */
    void Scope(FrameProfiler& InProfiler, const char* InName, const double InMs)
    {
        const Int32 lIndex = InProfiler.Open(InName);
        InProfiler.Close(lIndex, InMs);
    }
}

// =============================================================================
// FrameProfiler — shape
// =============================================================================

TEST_CASE("FrameProfiler: nothing is readable before the first Publish")
{
    FrameProfiler lProfiler;
    lProfiler.Open("Update");

    CHECK(lProfiler.IsEmpty());
    CHECK(lProfiler.Samples().empty());
}

TEST_CASE("FrameProfiler: samples come out in PRE-ORDER with the depth that draws the tree")
{
    // The reason Open records instead of Close: closing innermost-first would emit the children
    // ahead of their parent, and the panel would need a sort to undo it.
    FrameProfiler lProfiler;

    const Int32 lUpdate = lProfiler.Open("Update");
    const Int32 lWorlds = lProfiler.Open("WorldManager");
    const Int32 lGame   = lProfiler.Open("GameSubsystem");
    lProfiler.Close(lGame, 1.0);
    lProfiler.Close(lWorlds, 2.0);
    lProfiler.Close(lUpdate, 3.0);

    const Int32 lRender = lProfiler.Open("Render");
    lProfiler.Close(lRender, 4.0);

    lProfiler.Publish();

    const TDynArray<ScopeSample>& lSamples = lProfiler.Samples();
    REQUIRE(lSamples.size() == 4u);

    CHECK(lSamples[0].Name == doctest::String("Update"));
    CHECK(lSamples[0].Depth == 0);
    CHECK(lSamples[0].Milliseconds == doctest::Approx(3.0));

    CHECK(lSamples[1].Name == doctest::String("WorldManager"));
    CHECK(lSamples[1].Depth == 1);

    CHECK(lSamples[2].Name == doctest::String("GameSubsystem"));
    CHECK(lSamples[2].Depth == 2);

    // Back to the top after the nest closed — a depth that never came down would indent the rest
    // of the frame under a scope that had ended.
    CHECK(lSamples[3].Name == doctest::String("Render"));
    CHECK(lSamples[3].Depth == 0);
}

// =============================================================================
// FrameProfiler — repeated calls
// =============================================================================

TEST_CASE("FrameProfiler: a scope re-entered under the same parent is ONE row that counts calls")
{
    // The case a smoke run surfaced: a catch-up frame runs FixedUpdateAll once per step, so every
    // subsystem opens a scope N times. Without merging, one hitching frame is 90 rows of six
    // repeating names — a wall of noise exactly when the profiler is being read.
    FrameProfiler lProfiler;

    const Int32 lFixed = lProfiler.Open("FixedUpdate");
    for (int i = 0; i < 3; ++i)
    {
        const Int32 lA = lProfiler.Open("Physics");
        lProfiler.Close(lA, 1.0);
        const Int32 lB = lProfiler.Open("Camera");
        lProfiler.Close(lB, 0.5);
    }
    lProfiler.Close(lFixed, 4.5);

    lProfiler.Publish();

    const TDynArray<ScopeSample>& lSamples = lProfiler.Samples();
    REQUIRE(lSamples.size() == 3u);

    CHECK(lSamples[0].Name == doctest::String("FixedUpdate"));
    CHECK(lSamples[0].Calls == 1u);

    // Time ACCUMULATES across the calls; the row is the frame's total, not the last call's.
    CHECK(lSamples[1].Name == doctest::String("Physics"));
    CHECK(lSamples[1].Calls == 3u);
    CHECK(lSamples[1].Milliseconds == doctest::Approx(3.0));
    CHECK(lSamples[1].Depth == 1);

    CHECK(lSamples[2].Name == doctest::String("Camera"));
    CHECK(lSamples[2].Calls == 3u);
    CHECK(lSamples[2].Milliseconds == doctest::Approx(1.5));
}

TEST_CASE("FrameProfiler: the same name under DIFFERENT parents stays two rows")
{
    // Merging by name alone would fold a subsystem's Update cost into its Render cost.
    FrameProfiler lProfiler;

    const Int32 lUpdate = lProfiler.Open("Update");
    const Int32 lU1     = lProfiler.Open("Renderer");
    lProfiler.Close(lU1, 1.0);
    lProfiler.Close(lUpdate, 1.0);

    const Int32 lRender = lProfiler.Open("Render");
    const Int32 lR1     = lProfiler.Open("Renderer");
    lProfiler.Close(lR1, 5.0);
    lProfiler.Close(lRender, 5.0);

    lProfiler.Publish();

    const TDynArray<ScopeSample>& lSamples = lProfiler.Samples();
    REQUIRE(lSamples.size() == 4u);

    CHECK(lSamples[1].Name == doctest::String("Renderer"));
    CHECK(lSamples[1].Milliseconds == doctest::Approx(1.0));
    CHECK(lSamples[3].Name == doctest::String("Renderer"));
    CHECK(lSamples[3].Milliseconds == doctest::Approx(5.0));
}

TEST_CASE("FrameProfiler: merging is by TEXT, so two literals spelling the same name are one row")
{
    FrameProfiler lProfiler;

    const char  lFirst[]  = "Physics";
    const char  lSecond[] = "Physics";   // distinct storage, same text

    const Int32 lA = lProfiler.Open(lFirst);
    lProfiler.Close(lA, 1.0);
    const Int32 lB = lProfiler.Open(lSecond);
    lProfiler.Close(lB, 2.0);

    lProfiler.Publish();

    REQUIRE(lProfiler.Samples().size() == 1u);
    CHECK(lProfiler.Samples()[0].Calls == 2u);
    CHECK(lProfiler.Samples()[0].Milliseconds == doctest::Approx(3.0));
}

TEST_CASE("FrameProfiler: siblings sit at the SAME depth")
{
    FrameProfiler lProfiler;

    const Int32 lParent = lProfiler.Open("Update");
    const Int32 lA      = lProfiler.Open("A");
    lProfiler.Close(lA, 1.0);
    const Int32 lB      = lProfiler.Open("B");
    lProfiler.Close(lB, 2.0);
    lProfiler.Close(lParent, 3.0);

    lProfiler.Publish();

    REQUIRE(lProfiler.Samples().size() == 3u);
    CHECK(lProfiler.Samples()[1].Depth == 1);
    CHECK(lProfiler.Samples()[2].Depth == 1);
}

// =============================================================================
// FrameProfiler — the frame boundary
// =============================================================================

TEST_CASE("FrameProfiler: Publish hands over the PREVIOUS frame and starts the next one empty")
{
    // This is what lets the Stats panel — which draws in the middle of the wall-clock frame — read
    // a complete frame instead of a half-filled one.
    FrameProfiler lProfiler;

    const Int32 lFirst = lProfiler.Open("FrameOne");
    lProfiler.Close(lFirst, 1.0);
    lProfiler.Publish();

    REQUIRE(lProfiler.Samples().size() == 1u);
    CHECK(lProfiler.Samples()[0].Name == doctest::String("FrameOne"));

    // Mid-recording, the reader must still see frame one and NOT the scope now open.
    const Int32 lSecond = lProfiler.Open("FrameTwo");
    REQUIRE(lProfiler.Samples().size() == 1u);
    CHECK(lProfiler.Samples()[0].Name == doctest::String("FrameOne"));

    lProfiler.Close(lSecond, 2.0);
    lProfiler.Publish();

    REQUIRE(lProfiler.Samples().size() == 1u);
    CHECK(lProfiler.Samples()[0].Name == doctest::String("FrameTwo"));
}

TEST_CASE("FrameProfiler: an empty frame publishes empty rather than repeating the last one")
{
    FrameProfiler lProfiler;

    const Int32 lIndex = lProfiler.Open("Update");
    lProfiler.Close(lIndex, 1.0);
    lProfiler.Publish();
    REQUIRE(lProfiler.Samples().size() == 1u);

    lProfiler.Publish();
    CHECK(lProfiler.Samples().empty());
}

TEST_CASE("FrameProfiler: Publish resets depth, so an unclosed scope cannot indent the next frame")
{
    FrameProfiler lProfiler;

    lProfiler.Open("Leaked");   // never closed
    lProfiler.Publish();

    const Int32 lNext = lProfiler.Open("Update");
    lProfiler.Close(lNext, 1.0);
    lProfiler.Publish();

    REQUIRE(lProfiler.Samples().size() == 1u);
    CHECK(lProfiler.Samples()[0].Depth == 0);
}

TEST_CASE("FrameProfiler: Close ignores an out-of-range index")
{
    FrameProfiler lProfiler;

    lProfiler.Close(-1, 5.0);
    lProfiler.Close(99, 5.0);

    lProfiler.Publish();
    CHECK(lProfiler.Samples().empty());
}

// =============================================================================
// ScopedStat
// =============================================================================

TEST_CASE("ScopedStat: a null profiler is a no-op, so an unattached manager needs no branch")
{
    // ISubsystemManager ticks with m_Profiler == nullptr whenever nobody attached one (every bare
    // manager in a test), and it must not have to check.
    {
        const ScopedStat lStat(nullptr, "Nowhere");
    }

    CHECK(true);   // reaching here without a crash IS the assertion
}

TEST_CASE("ScopedStat: records the scope it wrapped, nested by block")
{
    FrameProfiler lProfiler;

    {
        const ScopedStat lOuter(&lProfiler, "Outer");
        {
            const ScopedStat lInner(&lProfiler, "Inner");
        }
    }

    lProfiler.Publish();

    REQUIRE(lProfiler.Samples().size() == 2u);
    CHECK(lProfiler.Samples()[0].Name == doctest::String("Outer"));
    CHECK(lProfiler.Samples()[0].Depth == 0);
    CHECK(lProfiler.Samples()[1].Name == doctest::String("Inner"));
    CHECK(lProfiler.Samples()[1].Depth == 1);

    // A real elapsed time, not the 0.0 the slot was reserved with.
    CHECK(lProfiler.Samples()[0].Milliseconds >= 0.0);
}

// =============================================================================
// TStatsHistory
// =============================================================================

TEST_CASE("TStatsHistory: empty answers zero for every statistic")
{
    const TStatsHistory<4> lHistory;

    CHECK(lHistory.Count() == 0u);
    CHECK(lHistory.Offset() == 0u);
    CHECK(lHistory.Average() == doctest::Approx(0.f));
    CHECK(lHistory.Min() == doctest::Approx(0.f));
    CHECK(lHistory.Max() == doctest::Approx(0.f));
}

TEST_CASE("TStatsHistory: a partly-filled ring reads in order from index 0")
{
    TStatsHistory<4> lHistory;
    lHistory.Push(10.f);
    lHistory.Push(20.f);

    CHECK(lHistory.Count() == 2u);
    CHECK(lHistory.Offset() == 0u);   // not m_Next — the samples are simply [0, Count)
    CHECK(lHistory.At(0) == doctest::Approx(10.f));
    CHECK(lHistory.At(1) == doctest::Approx(20.f));
    CHECK(lHistory.Average() == doctest::Approx(15.f));
    CHECK(lHistory.Min() == doctest::Approx(10.f));
    CHECK(lHistory.Max() == doctest::Approx(20.f));
}

TEST_CASE("TStatsHistory: WRAPPING drops the oldest and Offset names the new one")
{
    // The case the graph gets wrong if Offset is confused with the write cursor: a plot starting at
    // the wrong sample scrolls backwards and the whole readout is subtly a lie.
    TStatsHistory<4> lHistory;
    for (const float lValue : { 1.f, 2.f, 3.f, 4.f, 5.f, 6.f })
    {
        lHistory.Push(lValue);
    }

    REQUIRE(lHistory.Count() == 4u);
    CHECK(lHistory.Offset() == 2u);

    // Oldest to newest: 3, 4, 5, 6 — the first two pushes are gone.
    CHECK(lHistory.At(0) == doctest::Approx(3.f));
    CHECK(lHistory.At(1) == doctest::Approx(4.f));
    CHECK(lHistory.At(2) == doctest::Approx(5.f));
    CHECK(lHistory.At(3) == doctest::Approx(6.f));

    CHECK(lHistory.Average() == doctest::Approx(4.5f));
    CHECK(lHistory.Min() == doctest::Approx(3.f));
    CHECK(lHistory.Max() == doctest::Approx(6.f));
}

TEST_CASE("TStatsHistory: exactly full wraps Offset back to zero")
{
    TStatsHistory<4> lHistory;
    for (const float lValue : { 1.f, 2.f, 3.f, 4.f })
    {
        lHistory.Push(lValue);
    }

    CHECK(lHistory.Count() == lHistory.Capacity());
    CHECK(lHistory.Offset() == 0u);
    CHECK(lHistory.At(0) == doctest::Approx(1.f));
    CHECK(lHistory.At(3) == doctest::Approx(4.f));
}

TEST_CASE("TStatsHistory: At past the end answers zero rather than reading a stale slot")
{
    TStatsHistory<4> lHistory;
    lHistory.Push(7.f);

    CHECK(lHistory.At(1) == doctest::Approx(0.f));
    CHECK(lHistory.At(99) == doctest::Approx(0.f));
}

TEST_CASE("TStatsHistory: Clear empties it back to the fresh state")
{
    TStatsHistory<4> lHistory;
    lHistory.Push(1.f);
    lHistory.Push(2.f);
    lHistory.Clear();

    CHECK(lHistory.Count() == 0u);
    CHECK(lHistory.Offset() == 0u);
    CHECK(lHistory.Average() == doctest::Approx(0.f));
}

// =============================================================================
// StatsDisplay — holding the layout still
// =============================================================================

TEST_CASE("StatsDisplay: an empty display takes the first frame wholesale")
{
    FrameProfiler lProfiler;
    Scope(lProfiler, "Update", 2.0);
    Scope(lProfiler, "Render", 4.0);
    lProfiler.Publish();

    FrameStats lStats;
    lStats.FrameMs  = 16.0;
    lStats.Profiler = lProfiler;

    Editor::StatsDisplay lDisplay;
    lDisplay.Update(lStats);

    REQUIRE(lDisplay.Scopes().size() == 2u);
    CHECK(lDisplay.FrameMs() == doctest::Approx(16.0));
    CHECK(lDisplay.TopLevelMs() == doctest::Approx(6.0));
    CHECK(lDisplay.UnmeasuredMs() == doctest::Approx(10.0));
}

TEST_CASE("StatsDisplay: a scope that did not run KEEPS its row at zero")
{
    // The glitch this type exists for: a frame whose accumulator took no fixed step has no
    // FixedUpdate children, and every row below them jumps up and back.
    FrameProfiler lFull;
    const Int32 lFixed = lFull.Open("FixedUpdate");
    Scope(lFull, "Physics", 1.0);
    lFull.Close(lFixed, 1.0);
    Scope(lFull, "Render", 4.0);
    lFull.Publish();

    FrameStats lStats;
    lStats.FrameMs  = 16.0;
    lStats.Profiler = lFull;

    Editor::StatsDisplay lDisplay;
    lDisplay.Update(lStats);
    REQUIRE(lDisplay.Scopes().size() == 3u);

    // Next frame: no step ran, so Physics is absent from the snapshot entirely.
    FrameProfiler lSparse;
    const Int32 lFixed2 = lSparse.Open("FixedUpdate");
    lSparse.Close(lFixed2, 0.0);
    Scope(lSparse, "Render", 5.0);
    lSparse.Publish();

    FrameStats lNext;
    lNext.FrameMs  = 16.0;
    lNext.Profiler = lSparse;
    lDisplay.Update(lNext);

    REQUIRE(lDisplay.Scopes().size() == 3u);
    CHECK(lDisplay.Scopes()[1].Name == doctest::String("Physics"));
    CHECK(lDisplay.Scopes()[1].Milliseconds == doctest::Approx(0.0));
    CHECK(lDisplay.Scopes()[1].Calls == 0u);

    // And the row after it still holds the NEW value, not a stale one.
    CHECK(lDisplay.Scopes()[2].Name == doctest::String("Render"));
    CHECK(lDisplay.Scopes()[2].Milliseconds == doctest::Approx(5.0));
}

TEST_CASE("StatsDisplay: the same name under two parents keeps two rows with their own times")
{
    // Folding by name would post Render's cost onto Update's row. The match is an ordered
    // subsequence for exactly this reason.
    FrameProfiler lProfiler;

    const Int32 lUpdate = lProfiler.Open("Update");
    Scope(lProfiler, "Renderer", 1.0);
    lProfiler.Close(lUpdate, 1.0);

    const Int32 lRender = lProfiler.Open("Render");
    Scope(lProfiler, "Renderer", 5.0);
    lProfiler.Close(lRender, 5.0);

    lProfiler.Publish();

    FrameStats lStats;
    lStats.FrameMs  = 16.0;
    lStats.Profiler = lProfiler;

    Editor::StatsDisplay lDisplay;
    lDisplay.Update(lStats);
    lDisplay.Update(lStats);   // fold onto itself — the path that would collapse them

    REQUIRE(lDisplay.Scopes().size() == 4u);
    CHECK(lDisplay.Scopes()[1].Milliseconds == doctest::Approx(1.0));
    CHECK(lDisplay.Scopes()[3].Milliseconds == doctest::Approx(5.0));

    // Depth 0 only — Update and Render, not the nested Renderer rows counted twice.
    CHECK(lDisplay.TopLevelMs() == doctest::Approx(6.0));
}

TEST_CASE("StatsDisplay: a genuinely NEW scope rebuilds rather than folding")
{
    FrameProfiler lFirst;
    Scope(lFirst, "Update", 1.0);
    lFirst.Publish();

    FrameStats lStats;
    lStats.FrameMs  = 16.0;
    lStats.Profiler = lFirst;

    Editor::StatsDisplay lDisplay;
    lDisplay.Update(lStats);
    REQUIRE(lDisplay.Scopes().size() == 1u);

    // A module registered a subsystem: the frame is no longer a subsequence of the display.
    FrameProfiler lSecond;
    Scope(lSecond, "NewSystem", 0.5);
    Scope(lSecond, "Update", 1.0);
    lSecond.Publish();

    FrameStats lNext;
    lNext.FrameMs  = 16.0;
    lNext.Profiler = lSecond;
    lDisplay.Update(lNext);

    REQUIRE(lDisplay.Scopes().size() == 2u);
    CHECK(lDisplay.Scopes()[0].Name == doctest::String("NewSystem"));
}

TEST_CASE("StatsDisplay: Clear drops a dead world's rows instead of holding them at zero")
{
    FrameProfiler lProfiler;
    Scope(lProfiler, "Update", 1.0);
    lProfiler.Publish();

    FrameStats lStats;
    lStats.FrameMs  = 16.0;
    lStats.Profiler = lProfiler;

    Editor::StatsDisplay lDisplay;
    lDisplay.Update(lStats);
    lDisplay.Clear();

    CHECK(lDisplay.Scopes().empty());
    CHECK(lDisplay.FrameMs() == doctest::Approx(0.0));
    CHECK(lDisplay.UnmeasuredMs() == doctest::Approx(0.0));
}

TEST_CASE("StatsDisplay: the unmeasured remainder never goes negative")
{
    // Measured longer than the frame is possible across a publish boundary; a negative "Other" row
    // would read as nonsense.
    FrameProfiler lProfiler;
    Scope(lProfiler, "Present", 20.0);
    lProfiler.Publish();

    FrameStats lStats;
    lStats.FrameMs  = 16.0;
    lStats.Profiler = lProfiler;

    Editor::StatsDisplay lDisplay;
    lDisplay.Update(lStats);

    CHECK(lDisplay.UnmeasuredMs() == doctest::Approx(0.0));
}

// =============================================================================
// FrameProfiler — named counters
// =============================================================================

TEST_CASE("FrameProfiler: a counter ACCUMULATES within the frame and publishes with the scopes")
{
    // Counters and scopes must cross the frame boundary TOGETHER, or a reader sees a draw-call
    // count from one frame beside a Render time from another.
    FrameProfiler lProfiler;

    lProfiler.AddCount("Draw Calls", 1);
    lProfiler.AddCount("Quads", 40);
    lProfiler.AddCount("Draw Calls", 2);   // a second producer, or a second pass

    CHECK(lProfiler.Counters().empty());   // not readable until published

    lProfiler.Publish();

    REQUIRE(lProfiler.Counters().size() == 2u);
    CHECK(lProfiler.Counters()[0].Name == doctest::String("Draw Calls"));
    CHECK(lProfiler.Counters()[0].Value == 3u);
    CHECK(lProfiler.Counters()[1].Name == doctest::String("Quads"));
    CHECK(lProfiler.Counters()[1].Value == 40u);
}

TEST_CASE("FrameProfiler: counters do not survive a frame")
{
    FrameProfiler lProfiler;

    lProfiler.AddCount("Quads", 10);
    lProfiler.Publish();
    REQUIRE(lProfiler.Counters().size() == 1u);

    lProfiler.Publish();
    CHECK(lProfiler.Counters().empty());
}

TEST_CASE("FrameProfiler: counters merge by TEXT, like scope names do")
{
    FrameProfiler lProfiler;

    const char lFirst[]  = "Quads";
    const char lSecond[] = "Quads";   // distinct storage, same text

    lProfiler.AddCount(lFirst, 3);
    lProfiler.AddCount(lSecond, 4);
    lProfiler.Publish();

    REQUIRE(lProfiler.Counters().size() == 1u);
    CHECK(lProfiler.Counters()[0].Value == 7u);
}

TEST_CASE("FrameProfiler: IsEmpty accounts for counters, not just scopes")
{
    // A frame that only submitted counters has still measured something — and the one-shot
    // "stats live" log gates on this.
    FrameProfiler lProfiler;

    lProfiler.AddCount("Quads", 1);
    lProfiler.Publish();

    CHECK_FALSE(lProfiler.IsEmpty());
}

TEST_CASE("StatsDisplay: a counter that stops being submitted holds its row at zero")
{
    FrameProfiler lProfiler;
    lProfiler.AddCount("Draw Calls", 2);
    lProfiler.AddCount("Quads", 99);
    lProfiler.Publish();

    FrameStats lStats;
    lStats.FrameMs  = 16.0;
    lStats.Profiler = lProfiler;

    Editor::StatsDisplay lDisplay;
    lDisplay.Update(lStats);
    REQUIRE(lDisplay.Counters().size() == 2u);

    // Next frame drew nothing at all — the renderer still submits, but as zeros.
    FrameProfiler lEmpty;
    lEmpty.AddCount("Draw Calls", 0);
    lEmpty.Publish();

    FrameStats lNext;
    lNext.FrameMs  = 16.0;
    lNext.Profiler = lEmpty;
    lDisplay.Update(lNext);

    REQUIRE(lDisplay.Counters().size() == 2u);
    CHECK(lDisplay.Counters()[0].Value == 0u);
    CHECK(lDisplay.Counters()[1].Name == doctest::String("Quads"));
    CHECK(lDisplay.Counters()[1].Value == 0u);   // held, not dropped
}
