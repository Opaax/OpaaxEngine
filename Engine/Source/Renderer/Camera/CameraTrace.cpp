#include "Renderer/Camera/CameraTrace.h"

#include "Core/Algo/Algo.h"
#include "Core/Maths/Maths.h"

namespace Opaax::CameraTrace
{
    namespace
    {
        /** A size change smaller than this is no zoom, world units. */
        constexpr float ZOOM_EPSILON = 1.0e-5f;

        /** Pixels a world unit spans in a view InViewSize tall (half-extent), drawn InHeightPx tall. */
        float PixelsPerUnit(const float InViewSize, const float InHeightPx)
        {
            return InViewSize > 0.f ? InHeightPx / (2.f * InViewSize) : 0.f;
        }

        float Length(const Vector2F& InVector)
        {
            return Maths::Sqrt(InVector.x * InVector.x + InVector.y * InVector.y);
        }

        /** Where the target was from the view's centre, world units along the view's own axes (its roll undone). */
        Vector2F TargetFromView(const CameraTraceSample& InSample)
        {
            const Vector2F lFromView = InSample.TargetPosition - InSample.ViewPosition;
            if (InSample.ViewRotation == 0.f)
            {
                return lFromView;
            }

            const float lRad = Maths::DegreesToRadians(-InSample.ViewRotation);
            const float lCos = Maths::Cos(lRad);
            const float lSin = Maths::Sin(lRad);
            return { lCos * lFromView.x - lSin * lFromView.y, lSin * lFromView.x + lCos * lFromView.y };
        }

        /** Where the target was drawn from the view's centre, pixels. */
        Vector2F TargetOnScreen(const CameraTraceSample& InSample, const float InHeightPx)
        {
            return TargetFromView(InSample) * PixelsPerUnit(InSample.ViewSize, InHeightPx);
        }

        /** InTo - InFrom the short way round, degrees in [-180, 180]. */
        float AngleBetween(const float InFrom, const float InTo)
        {
            return Maths::Remainder(InTo - InFrom, 360.f);
        }

        /** Keeps the larger of InValue and InOutLargest, with the frame it came from. */
        void KeepLargest(const float InValue, const Uint32 InFrame, float& InOutLargest, Uint32& OutFrame)
        {
            if (InValue > InOutLargest)
            {
                InOutLargest = InValue;
                OutFrame     = InFrame;
            }
        }
    }

    float Drift(const CameraTraceSample& InBefore, const CameraTraceSample& InSample, const float InHeightPx)
    {
        if (!InBefore.bHasTarget || !InSample.bHasTarget || InSample.Camera != InBefore.Camera || InSample.bCut)
        {
            return 0.f;
        }

        // A move in the world, shown at this frame's scale: a zoom alone moves nothing.
        return Length(TargetFromView(InSample) - TargetFromView(InBefore)) * PixelsPerUnit(InSample.ViewSize, InHeightPx);
    }

    CameraTraceSummary Summarize(const TDynArray<CameraTraceSample>& InSamples, const float InHeightPx)
    {
        CameraTraceSummary lSummary;
        lSummary.Frames = static_cast<Uint32>(InSamples.size());

        for (Uint64 lIndex = 0; lIndex < InSamples.size(); ++lIndex)
        {
            lSummary.MostSteps   = Maths::Max(lSummary.MostSteps, InSamples[lIndex].Steps);
            lSummary.FewestSteps = lIndex == 0 ? InSamples[lIndex].Steps : Maths::Min(lSummary.FewestSteps, InSamples[lIndex].Steps);
        }

        // The view's move in the world on the frame before, for the jitter; none after a switch.
        Vector2F lLastMove      = { 0.f, 0.f };
        bool     bHasLastMove   = false;
        Int32    lZoomDirection = 0;

        for (Uint64 lIndex = 1; lIndex < InSamples.size(); ++lIndex)
        {
            const CameraTraceSample& lBefore = InSamples[lIndex - 1];
            const CameraTraceSample& lSample = InSamples[lIndex];
            const Uint32             lFrame  = static_cast<Uint32>(lIndex);

            if (lSample.GameTime < lBefore.GameTime - SAME_TIME)
            {
                ++lSummary.TimeReversals;
            }

            // Moves in the world, shown at this frame's scale.
            const float    lPixels = PixelsPerUnit(lSample.ViewSize, InHeightPx);
            const Vector2F lMove   = lSample.ViewPosition - lBefore.ViewPosition;
            const float    lMoved  = Length(lMove) * lPixels;

            // Another camera, or a cut: a jump on purpose, not a move. How much a switch changed the
            // view's move tells a cut from a blend.
            if (lSample.Camera != lBefore.Camera || lSample.bCut)
            {
                if (lSample.Camera != lBefore.Camera && !lSample.bCut && bHasLastMove)
                {
                    KeepLargest(Length(lMove - lLastMove) * lPixels, lFrame, lSummary.SwitchJump, lSummary.SwitchJumpFrame);
                }
                lSummary.CameraSwitches += lSample.Camera != lBefore.Camera ? 1 : 0;
                lSummary.Cuts           += lSample.bCut ? 1 : 0;
                bHasLastMove   = false;
                lZoomDirection = 0;
                continue;
            }
            KeepLargest(lMoved, lFrame, lSummary.LargestMove, lSummary.LargestMoveFrame);

            if (bHasLastMove)
            {
                KeepLargest(Length(lMove - lLastMove) * lPixels, lFrame, lSummary.MotionJitter,
                            lSummary.MotionJitterFrame);
            }
            lLastMove    = lMove;
            bHasLastMove = true;

            if (Maths::Abs(lSample.GameTime - lBefore.GameTime) <= SAME_TIME && lMoved > STILL_PIXELS)
            {
                ++lSummary.StillMoves;
            }

            KeepLargest(Drift(lBefore, lSample, InHeightPx), lFrame, lSummary.TargetDrift, lSummary.TargetDriftFrame);

            const float lZoom      = lSample.ViewSize - lBefore.ViewSize;
            const Int32 lDirection = lZoom > ZOOM_EPSILON ? 1 : (lZoom < -ZOOM_EPSILON ? -1 : 0);
            if (lDirection != 0)
            {
                if (lZoomDirection != 0 && lDirection != lZoomDirection)
                {
                    ++lSummary.ZoomReversals;
                }
                lZoomDirection = lDirection;
            }
        }

        return lSummary;
    }

    CameraTraceGap Compare(const TDynArray<CameraTraceSample>& InReference, const TDynArray<CameraTraceSample>& InTrace)
    {
        // The reference by game time, so a frame finds the reference's frame at any time.
        TDynArray<const CameraTraceSample*> lByTime;
        lByTime.reserve(InReference.size());
        for (const CameraTraceSample& lSample : InReference)
        {
            lByTime.push_back(&lSample);
        }
        Algo::Sort(lByTime, [](const CameraTraceSample* InLeft, const CameraTraceSample* InRight)
        {
            return InLeft->GameTime < InRight->GameTime;
        });

        const auto lEarlier = [](const CameraTraceSample* InOther, const double InValue)
        {
            return InOther->GameTime < InValue;
        };

        const auto lFindAt = [&lByTime, &lEarlier](const double InTime, const OpaaxString& InCamera) -> const CameraTraceSample*
        {
            auto lAt = Algo::LowerBound(lByTime, InTime - SAME_TIME, lEarlier);

            for (; lAt != lByTime.end() && (*lAt)->GameTime <= InTime + SAME_TIME; ++lAt)
            {
                if ((*lAt)->Camera == InCamera)
                {
                    return *lAt;
                }
            }
            return nullptr;
        };

        // The reference's frames of InCamera nearest before and after InTime, looked for within InSpan.
        const auto lFindAround = [&lByTime, &lEarlier](const double InTime, const OpaaxString& InCamera, const double InSpan,
                                                       const CameraTraceSample*& OutBefore, const CameraTraceSample*& OutAfter)
        {
            const auto lAt = Algo::LowerBound(lByTime, InTime, lEarlier);

            OutAfter = nullptr;
            for (auto lIt = lAt; lIt != lByTime.end() && (*lIt)->GameTime <= InTime + InSpan; ++lIt)
            {
                if ((*lIt)->Camera == InCamera) { OutAfter = *lIt; break; }
            }

            OutBefore = nullptr;
            for (auto lIt = lAt; lIt != lByTime.begin() && (*(lIt - 1))->GameTime >= InTime - InSpan;)
            {
                --lIt;
                if ((*lIt)->Camera == InCamera) { OutBefore = *lIt; break; }
            }
        };

        CameraTraceGap lGap;
        for (const CameraTraceSample& lSample : InTrace)
        {
            Vector2F lExpected         = { 0.f, 0.f };
            float    lExpectedSize     = 0.f;
            float    lExpectedRotation = 0.f;

            if (const CameraTraceSample* lSame = lFindAt(lSample.GameTime, lSample.Camera))
            {
                lExpected         = lSame->ViewPosition;
                lExpectedSize     = lSame->ViewSize;
                lExpectedRotation = lSame->ViewRotation;
            }
            else
            {
                // Between two frames of the reference no more than a step apart (a reference at the fixed
                // rate draws each step's end; a faster one draws within it), blended by time. Not across a cut.
                const CameraTraceSample* lFrom = nullptr;
                const CameraTraceSample* lTo   = nullptr;
                lFindAround(lSample.GameTime, lSample.Camera, lSample.StepDelta + SAME_TIME, lFrom, lTo);
                if (lFrom == nullptr || lTo == nullptr || lTo->bCut || lTo->GameTime - lFrom->GameTime > lSample.StepDelta + SAME_TIME)
                {
                    continue;
                }

                const float lAlpha = static_cast<float>((lSample.GameTime - lFrom->GameTime) / (lTo->GameTime - lFrom->GameTime));
                lExpected         = lFrom->ViewPosition + (lTo->ViewPosition - lFrom->ViewPosition) * lAlpha;
                lExpectedSize     = lFrom->ViewSize + (lTo->ViewSize - lFrom->ViewSize) * lAlpha;
                lExpectedRotation = lFrom->ViewRotation + AngleBetween(lFrom->ViewRotation, lTo->ViewRotation) * lAlpha;
            }

            ++lGap.Shared;

            const float lPosition = Length(lSample.ViewPosition - lExpected);
            const float lSize     = Maths::Abs(lSample.ViewSize - lExpectedSize);
            const float lRotation = Maths::Abs(AngleBetween(lExpectedRotation, lSample.ViewRotation));
            if (lPosition > lGap.Position || lSize > lGap.Size || lRotation > lGap.Rotation)
            {
                lGap.AtTime = lSample.GameTime;
            }
            lGap.Position = Maths::Max(lGap.Position, lPosition);
            lGap.Size     = Maths::Max(lGap.Size, lSize);
            lGap.Rotation = Maths::Max(lGap.Rotation, lRotation);
        }

        return lGap;
    }

    OpaaxString ToCsv(const TDynArray<CameraTraceSample>& InSamples, const float InHeightPx)
    {
        OpaaxString lText("frame,delta,steps,alpha,stepDelta,gameTime,cut,camera,viewX,viewY,viewSize,viewRotation,"
                          "targetX,targetY,targetPxX,targetPxY\n");

        for (Uint64 lIndex = 0; lIndex < InSamples.size(); ++lIndex)
        {
            const CameraTraceSample& lSample = InSamples[lIndex];

            lText += OpaaxString::Format("{},{},{},{},{},{},{},{},{},{},{},{}", lIndex, lSample.FrameDelta, lSample.Steps,
                                         lSample.Alpha, lSample.StepDelta, lSample.GameTime, lSample.bCut ? 1 : 0,
                                         lSample.Camera, lSample.ViewPosition.x, lSample.ViewPosition.y, lSample.ViewSize,
                                         lSample.ViewRotation);
            if (lSample.bHasTarget)
            {
                const Vector2F lOnScreen = TargetOnScreen(lSample, InHeightPx);
                lText += OpaaxString::Format(",{},{},{},{}\n", lSample.TargetPosition.x, lSample.TargetPosition.y,
                                             lOnScreen.x, lOnScreen.y);
            }
            else
            {
                lText += ",,,,\n";
            }
        }

        return lText;
    }
}
