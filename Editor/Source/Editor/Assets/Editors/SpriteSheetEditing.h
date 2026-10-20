#pragma once

#include "Core/Maths/MathTypes.h"   // Vector2F
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Renderer/Textures/SpriteSheet.h"

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
    }
}
