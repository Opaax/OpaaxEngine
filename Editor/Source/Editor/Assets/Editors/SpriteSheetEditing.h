#pragma once

#include "Core/Maths/Margin.h"
#include "Core/Maths/Maths.h"
#include "Core/Maths/MathTypes.h"   // Vector2F
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Editor/UI/EditorRectGeometry.h"   // ERectEdge
#include "Renderer/Textures/SpriteSheet.h"
#include "UI/UISlice.h"

namespace Opaax::Editor
{
    // =============================================================================
    // SheetEditing — what the sprite sheet editor does to a sheet, as pure functions so tests can check
    //   them without a window.
    // =============================================================================
    namespace SheetEditing
    {
        /**
         * Replaces InOutSheet's frames with its grid cut over a InTexWidth x InTexHeight texture. A default
         * frame past the new last one goes back to the first.
         * @return False when the grid cuts nothing: the frames are kept (a mistyped cell size would wipe them)
         */
        inline bool Slice(SpriteSheet& InOutSheet, const Uint32 InTexWidth, const Uint32 InTexHeight)
        {
            TDynArray<SpriteFrame> lSliced = SliceGrid(InOutSheet.Grid, InTexWidth, InTexHeight);
            if (lSliced.empty())
            {
                return false;
            }

            InOutSheet.Frames = Move(lSliced);

            if (InOutSheet.DefaultFrame >= InOutSheet.FrameCount())
            {
                InOutSheet.DefaultFrame = 0u;
            }

            return true;
        }

        /**
         * Names every unnamed frame "Frame_<index>", skipping a name already in use, and keeps the named
         * ones (pressing it twice changes nothing). Clips name frames, first match: a name must be unique.
         * @return How many frames were named
         */
        inline Uint32 AutoNameFrames(TDynArray<SpriteFrame>& InOutFrames)
        {
            auto lInUse = [&InOutFrames](const OpaaxStringID InName)
            {
                for (const SpriteFrame& lFrame : InOutFrames)
                {
                    if (lFrame.Name == InName)
                    {
                        return true;
                    }
                }

                return false;
            };

            Uint32 lNamed = 0u;

            for (Uint32 lIndex = 0; lIndex < InOutFrames.size(); ++lIndex)
            {
                if (InOutFrames[lIndex].Name.IsValid())
                {
                    continue;
                }

                OpaaxStringID lCandidate;
                for (Uint32 lSuffix = lIndex; ; ++lSuffix)
                {
                    lCandidate = OpaaxStringID(OpaaxString("Frame_") + OpaaxString::FromUInt(lSuffix));
                    if (!lInUse(lCandidate))
                    {
                        break;
                    }
                }

                InOutFrames[lIndex].Name = lCandidate;
                ++lNamed;
            }

            return lNamed;
        }

        /** The first frame whose rect holds InTexel (texture pixels, edges included), or -1. */
        inline Int32 FrameUnder(const TDynArray<SpriteFrame>& InFrames, const Vector2F InTexel)
        {
            for (Uint32 lIndex = 0; lIndex < InFrames.size(); ++lIndex)
            {
                const SpriteFrame& lFrame = InFrames[lIndex];

                if (InTexel.x >= lFrame.Offset.x && InTexel.x <= lFrame.Offset.x + lFrame.Size.x
                    && InTexel.y >= lFrame.Offset.y && InTexel.y <= lFrame.Offset.y + lFrame.Size.y)
                {
                    return static_cast<Int32>(lIndex);
                }
            }

            return -1;
        }

        // =============================================================================
        // A frame's slice border, as four lines over the frame. Positions are in texture pixels from
        //   the frame's top-left corner, Y down: Left at x = Left, Right at x = width - Right, Top at
        //   y = Top, Bottom at y = height - Bottom. A zero border's line lies on the frame's edge.
        // =============================================================================

        /**
         * The border lines a press at InCursor takes: the nearest vertical line (Left or Right) and the
         * nearest horizontal one (Top or Bottom) within InGrab, together as a corner when both are near.
         * @param InCursor Texture pixels from the frame's top-left corner
         * @param InGrab   How far from a line still takes it, in texture pixels
         * @return None when no line is near (the middle of a cell)
         */
        inline ERectEdge BorderLinesUnder(const Margin& InBorder, const Vector2F InFrameSize, const Vector2F InCursor,
                                          const float InGrab) noexcept
        {
            // Outside the frame (beyond the grab distance) takes nothing.
            if (InCursor.x < -InGrab || InCursor.y < -InGrab
                || InCursor.x > InFrameSize.x + InGrab || InCursor.y > InFrameSize.y + InGrab)
            {
                return ERectEdge::None;
            }

            const float lToLeft   = Maths::Abs(InCursor.x - InBorder.Left);
            const float lToRight  = Maths::Abs(InCursor.x - (InFrameSize.x - InBorder.Right));
            const float lToTop    = Maths::Abs(InCursor.y - InBorder.Top);
            const float lToBottom = Maths::Abs(InCursor.y - (InFrameSize.y - InBorder.Bottom));

            // The nearer of two lines on one axis: two lines on the same spot give the one the cursor is past.
            const bool bLeft   = lToLeft <= InGrab && (lToLeft < lToRight || (lToLeft == lToRight && InCursor.x <= InBorder.Left));
            const bool bRight  = !bLeft && lToRight <= InGrab;
            const bool bTop    = lToTop <= InGrab && (lToTop < lToBottom || (lToTop == lToBottom && InCursor.y <= InBorder.Top));
            const bool bBottom = !bTop && lToBottom <= InGrab;

            if (bTop && bLeft)     { return ERectEdge::TopLeft; }
            if (bTop && bRight)    { return ERectEdge::TopRight; }
            if (bBottom && bLeft)  { return ERectEdge::BottomLeft; }
            if (bBottom && bRight) { return ERectEdge::BottomRight; }
            if (bLeft)             { return ERectEdge::Left; }
            if (bRight)            { return ERectEdge::Right; }
            if (bTop)              { return ERectEdge::Top; }
            if (bBottom)           { return ERectEdge::Bottom; }
            return ERectEdge::None;
        }

        /**
         * InBorder with the lines InLines taken to the texture pixel under InCursor: whole pixels, inside
         * the frame, never past the opposite line. The other lines are kept as they are.
         * @param InCursor Texture pixels from the frame's top-left corner
         */
        inline Margin DragBorder(const Margin& InBorder, const Vector2F InFrameSize, const ERectEdge InLines,
                                 const Vector2F InCursor) noexcept
        {
            const auto lPixel = [](const float InValue) { return Maths::Floor(InValue + 0.5f); };
            const auto lClamp = [](const float InValue, const float InMax)
            {
                return Maths::Max(0.f, Maths::Min(InValue, Maths::Max(0.f, InMax)));
            };

            const float lX = lPixel(InCursor.x);
            const float lY = lPixel(InCursor.y);

            Margin lBorder = InBorder;

            if (InLines == ERectEdge::Left || InLines == ERectEdge::TopLeft || InLines == ERectEdge::BottomLeft)
            {
                lBorder.Left = lClamp(lX, InFrameSize.x - lBorder.Right);
            }
            else if (InLines == ERectEdge::Right || InLines == ERectEdge::TopRight || InLines == ERectEdge::BottomRight)
            {
                lBorder.Right = lClamp(InFrameSize.x - lX, InFrameSize.x - lBorder.Left);
            }

            if (InLines == ERectEdge::Top || InLines == ERectEdge::TopLeft || InLines == ERectEdge::TopRight)
            {
                lBorder.Top = lClamp(lY, InFrameSize.y - lBorder.Bottom);
            }
            else if (InLines == ERectEdge::Bottom || InLines == ERectEdge::BottomLeft || InLines == ERectEdge::BottomRight)
            {
                lBorder.Bottom = lClamp(InFrameSize.y - lY, InFrameSize.y - lBorder.Top);
            }

            return lBorder;
        }

        // =============================================================================
        // The sliced preview: what a UI image or button draws with a frame
        // =============================================================================

        /** One preview quad: a rect from the preview's top-left (Y down) and the texture's part it shows. */
        struct PreviewQuad
        {
            Vector2F Min   = { 0.f, 0.f };   // canvas units from the preview's top-left
            Vector2F Max   = { 0.f, 0.f };
            Vector2F UVMin = { 0.f, 0.f };   // fractions of the texture from its top-left: UVMin is the quad's top-left
            Vector2F UVMax = { 1.f, 1.f };
        };

        /**
         * InFrame drawn over a InPreviewSize rect at InBorderScale, sliced with its own border as UI images
         * and buttons slice it (BuildSlicedQuads), in the editor's orientation: Y down, UVs from the texture's
         * top-left. No border gives one quad.
         * @param InTextureSize The sheet's texture, in pixels
         */
        inline TDynArray<PreviewQuad> PreviewQuads(const SpriteFrame& InFrame, const Vector2F InTextureSize,
                                                   const Vector2F InPreviewSize, const float InBorderScale)
        {
            TDynArray<PreviewQuad> lOut;
            if (InTextureSize.x <= 0.f || InTextureSize.y <= 0.f || InFrame.Size.x <= 0.f || InFrame.Size.y <= 0.f
                || InPreviewSize.x <= 0.f || InPreviewSize.y <= 0.f)
            {
                return lOut;
            }

            // The UI's own maths, in its space: Y up, UVs 0..1 of the frame with V up.
            const Bounds2D    lRect = Bounds2D::FromMinMax({ 0.f, 0.f }, InPreviewSize);
            TDynArray<UIQuad> lQuads;
            if (InFrame.Border.IsZero())
            {
                lQuads.emplace_back().Bounds = lRect;
            }
            else
            {
                BuildSlicedQuads(lRect, InFrame.Border, InBorderScale, InFrame.Size, lQuads);
            }

            lOut.reserve(lQuads.size());
            for (const UIQuad& lQuad : lQuads)
            {
                const Vector2F lMin = lQuad.Bounds.Min();
                const Vector2F lMax = lQuad.Bounds.Max();

                PreviewQuad& lPreview = lOut.emplace_back();
                lPreview.Min = { lMin.x, InPreviewSize.y - lMax.y };
                lPreview.Max = { lMax.x, InPreviewSize.y - lMin.y };

                // V up in the frame becomes rows down from the texture's top.
                lPreview.UVMin = { (InFrame.Offset.x + lQuad.UVMin.x * InFrame.Size.x) / InTextureSize.x,
                                   (InFrame.Offset.y + (1.f - lQuad.UVMax.y) * InFrame.Size.y) / InTextureSize.y };
                lPreview.UVMax = { (InFrame.Offset.x + lQuad.UVMax.x * InFrame.Size.x) / InTextureSize.x,
                                   (InFrame.Offset.y + (1.f - lQuad.UVMin.y) * InFrame.Size.y) / InTextureSize.y };
            }

            return lOut;
        }
    }
}
