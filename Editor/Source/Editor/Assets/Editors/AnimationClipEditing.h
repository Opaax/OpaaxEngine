#pragma once

#include "Animation/Assets/AnimationClip.h"
#include "Core/Maths/Maths.h"   // Lerp
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Editor/UI/IEditorUIBackend.h"   // EditorImage
#include "Renderer/Textures/SpriteSheet.h"   // SpriteFrame

namespace Opaax::Editor
{
    // =============================================================================
    // ClipEditing — what the clip editor does to a clip and its preview, as pure functions so tests
    //   can check them without a window.
    // =============================================================================
    namespace ClipEditing
    {
        /** Appends a copy of the last step (its picture and hold), or a default step to an empty clip. */
        inline void AddStep(TDynArray<AnimationStep>& InOutSteps)
        {
            InOutSteps.emplace_back(InOutSteps.empty() ? AnimationStep{} : InOutSteps.back());
        }

        /**
         * Appends a step showing InTexturePath (a dropped texture), held as long as the last step.
         * @return False for an empty path (nothing added)
         */
        inline bool AddTextureStep(TDynArray<AnimationStep>& InOutSteps, const OpaaxString& InTexturePath)
        {
            if (InTexturePath.IsEmpty())
            {
                return false;
            }

            AnimationStep lStep;
            lStep.Texture.Path = InTexturePath;
            lStep.Hold         = InOutSteps.empty() ? 1u : InOutSteps.back().EffectiveHold();

            InOutSteps.emplace_back(Move(lStep));
            return true;
        }

        /**
         * InFull cropped to InFrame's pixel rect. Interpolated inside InFull's UVs, so a backend that
         * flips its images still shows the frame the right way up. A bad frame or texture size gives InFull.
         */
        inline EditorImage CropToFrame(const EditorImage& InFull, const SpriteFrame& InFrame,
                                       const Uint32 InTexWidth, const Uint32 InTexHeight)
        {
            const float lWidth  = static_cast<float>(InTexWidth);
            const float lHeight = static_cast<float>(InTexHeight);

            if (lWidth <= 0.f || lHeight <= 0.f || InFrame.Size.x <= 0.f || InFrame.Size.y <= 0.f)
            {
                return InFull;
            }

            EditorImage lCropped = InFull;

            lCropped.UV0.x = Maths::Lerp(InFull.UV0.x, InFull.UV1.x, InFrame.Offset.x / lWidth);
            lCropped.UV1.x = Maths::Lerp(InFull.UV0.x, InFull.UV1.x, (InFrame.Offset.x + InFrame.Size.x) / lWidth);
            lCropped.UV0.y = Maths::Lerp(InFull.UV0.y, InFull.UV1.y, InFrame.Offset.y / lHeight);
            lCropped.UV1.y = Maths::Lerp(InFull.UV0.y, InFull.UV1.y, (InFrame.Offset.y + InFrame.Size.y) / lHeight);

            return lCropped;
        }
    }
}
