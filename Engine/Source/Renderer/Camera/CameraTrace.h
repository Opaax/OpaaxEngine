#pragma once

#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // CameraTraceSample — one frame as the camera framed it, and where a followed target was drawn.
    // =============================================================================
    struct CameraTraceSample
    {
        /** The frame's delta, seconds: the clock's or the pacing's. */
        double FrameDelta = 0.0;

        /** The fixed steps the world ran this frame, how far into the next one it was drawn, and their length. */
        Uint32 Steps     = 0;
        double Alpha     = 0.0;
        double StepDelta = 0.0;

        /** The game time drawn: the time the world's steps simulated, plus alpha of a step. */
        double GameTime = 0.0;

        /** The camera that framed the world (its entity's name), and the view it gave. */
        OpaaxString Camera;
        Vector2F    ViewPosition = { 0.f, 0.f };
        float       ViewSize     = 0.f;

        /** Where the target was drawn, world units; only with bHasTarget. */
        bool     bHasTarget     = false;
        Vector2F TargetPosition = { 0.f, 0.f };
    };

    // =============================================================================
    // CameraTraceSummary — what a trace says about the camera's smoothness, in pixels of a view
    //   HeightPx tall. The drift and the jitter measure moves in the world, at the frame's scale: a
    //   zoom alone moves nothing. Each "largest" comes with the frame it happened on.
    // =============================================================================
    struct CameraTraceSummary
    {
        Uint32 Frames = 0;

        /** The largest move of the target against the view from one frame to the next: 0 for a camera locked on it. */
        float  TargetDrift      = 0.f;
        Uint32 TargetDriftFrame = 0;

        /**
         * The largest change of the view's move in the world from one frame to the next: 0 for a steady
         * pan at a steady rate.
         */
        float  MotionJitter      = 0.f;
        Uint32 MotionJitterFrame = 0;

        /** The largest move of the view in one frame. */
        float  LargestMove      = 0.f;
        Uint32 LargestMoveFrame = 0;

        /** Frames drawn at an earlier game time than the frame before. */
        Uint32 TimeReversals = 0;

        /** Frames where the view moved while the game time did not: a paused world must hold still. */
        Uint32 StillMoves = 0;

        /** Times the zoom turned back (grew after shrinking, or the reverse). */
        Uint32 ZoomReversals = 0;

        /** Frames where another camera framed the world. Moves across a switch are not counted above. */
        Uint32 CameraSwitches = 0;
    };

    // =============================================================================
    // CameraTraceGap — how far a trace is from where a reference trace says its camera was.
    // =============================================================================
    struct CameraTraceGap
    {
        /**
         * Frames of the trace the reference could place: same camera, and the reference drew that time or
         * both ends of the frame's step.
         */
        Uint32 Shared = 0;

        /** The largest distance between their views (world units) and sizes, and where. */
        float  Position = 0.f;
        float  Size     = 0.f;
        double AtTime   = 0.0;
    };

    namespace CameraTrace
    {
        /** Two game times closer than this are the same moment, seconds. */
        inline constexpr double SAME_TIME = 1.0e-6;

        /** The view moving less than this is still, pixels. */
        inline constexpr float STILL_PIXELS = 0.001f;

        /** What InSamples say, measured in pixels of a view InHeightPx tall. */
        CameraTraceSummary Summarize(const TDynArray<CameraTraceSample>& InSamples, float InHeightPx);

        /**
         * How far each frame of InTrace is from where InReference says its camera was at that game time:
         * the reference's frame at that time, or its frames at both ends of the frame's step blended by its
         * alpha (a view drawn between steps is a blend of the two). The reference is a steady pacing at
         * the fixed rate or a multiple of it, so it draws every step's end; InTrace may be any pacing.
         */
        CameraTraceGap Compare(const TDynArray<CameraTraceSample>& InReference,
                               const TDynArray<CameraTraceSample>& InTrace);

        /** InSamples as CSV, a row a frame, the target also in pixels from the view's centre (InHeightPx tall). */
        OpaaxString ToCsv(const TDynArray<CameraTraceSample>& InSamples, float InHeightPx);
    }
}
