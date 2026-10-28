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

        /** Where the target was drawn from the view's centre, pixels. */
        Vector2F TargetOnScreen(const CameraTraceSample& InSample, const float InHeightPx)
        {
            return (InSample.TargetPosition - InSample.ViewPosition) * PixelsPerUnit(InSample.ViewSize, InHeightPx);
        }

        /** Where the target was from the view's centre, world units. */
        Vector2F TargetFromView(const CameraTraceSample& InSample)
        {
            return InSample.TargetPosition - InSample.ViewPosition;
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

    CameraTraceSummary Summarize(const TDynArray<CameraTraceSample>& InSamples, const float InHeightPx)
    {
        CameraTraceSummary lSummary;
        lSummary.Frames = static_cast<Uint32>(InSamples.size());

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

            // Another camera: a cut, not a move of this one.
            if (lSample.Camera != lBefore.Camera)
            {
                ++lSummary.CameraSwitches;
                bHasLastMove   = false;
                lZoomDirection = 0;
                continue;
            }

            // Moves in the world, shown at this frame's scale.
            const float    lPixels = PixelsPerUnit(lSample.ViewSize, InHeightPx);
            const Vector2F lMove   = lSample.ViewPosition - lBefore.ViewPosition;
            const float    lMoved  = Length(lMove) * lPixels;
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

            if (lSample.bHasTarget && lBefore.bHasTarget)
            {
                const float lDrift = Length(TargetFromView(lSample) - TargetFromView(lBefore)) * lPixels;
                KeepLargest(lDrift, lFrame, lSummary.TargetDrift, lSummary.TargetDriftFrame);
            }

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

        const auto lFindAt = [&lByTime](const double InTime, const OpaaxString& InCamera) -> const CameraTraceSample*
        {
            const auto lEarlier = [](const CameraTraceSample* InOther, const double InValue)
            {
                return InOther->GameTime < InValue;
            };

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

        CameraTraceGap lGap;
        for (const CameraTraceSample& lSample : InTrace)
        {
            Vector2F lExpected     = { 0.f, 0.f };
            float    lExpectedSize = 0.f;

            if (const CameraTraceSample* lSame = lFindAt(lSample.GameTime, lSample.Camera))
            {
                lExpected     = lSame->ViewPosition;
                lExpectedSize = lSame->ViewSize;
            }
            else
            {
                // Between two steps: the reference's view at both ends of the step, blended as this frame was.
                const double             lStart = lSample.GameTime - lSample.Alpha * lSample.StepDelta;
                const CameraTraceSample* lFrom = lSample.StepDelta > 0.0 ? lFindAt(lStart, lSample.Camera) : nullptr;
                const CameraTraceSample* lTo   = lFrom != nullptr ? lFindAt(lStart + lSample.StepDelta, lSample.Camera)
                                                                  : nullptr;
                if (lTo == nullptr)
                {
                    continue;
                }

                const float lAlpha = static_cast<float>(lSample.Alpha);
                lExpected     = lFrom->ViewPosition + (lTo->ViewPosition - lFrom->ViewPosition) * lAlpha;
                lExpectedSize = lFrom->ViewSize + (lTo->ViewSize - lFrom->ViewSize) * lAlpha;
            }

            ++lGap.Shared;

            const float lPosition = Length(lSample.ViewPosition - lExpected);
            const float lSize     = Maths::Abs(lSample.ViewSize - lExpectedSize);
            if (lPosition > lGap.Position || lSize > lGap.Size)
            {
                lGap.AtTime = lSample.GameTime;
            }
            lGap.Position = Maths::Max(lGap.Position, lPosition);
            lGap.Size     = Maths::Max(lGap.Size, lSize);
        }

        return lGap;
    }

    OpaaxString ToCsv(const TDynArray<CameraTraceSample>& InSamples, const float InHeightPx)
    {
        OpaaxString lText("frame,delta,steps,alpha,stepDelta,gameTime,camera,viewX,viewY,viewSize,targetX,targetY,targetPxX,"
                          "targetPxY\n");

        for (Uint64 lIndex = 0; lIndex < InSamples.size(); ++lIndex)
        {
            const CameraTraceSample& lSample = InSamples[lIndex];

            lText += OpaaxString::Format("{},{},{},{},{},{},{},{},{},{}", lIndex, lSample.FrameDelta, lSample.Steps,
                                         lSample.Alpha, lSample.StepDelta, lSample.GameTime, lSample.Camera,
                                         lSample.ViewPosition.x, lSample.ViewPosition.y, lSample.ViewSize);
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
