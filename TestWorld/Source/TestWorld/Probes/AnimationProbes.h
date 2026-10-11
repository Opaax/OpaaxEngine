#pragma once

#include <algorithm>

#include "Animation/SpriteAnimatorComponent.h"
#include "Core/String/OpaaxStringJson.h"
#include "Renderer/Components/SpriteComponent.h"
#include "World/Behaviour/Behaviour.h"

namespace TestWorld
{
    // =============================================================================
    // AnimProbe — watches what its SpriteAnimator puts in its Sprite: how many different frames it
    //   showed, and the last frame, texture and sheet. With SwitchTo, plays that clip of the library
    //   SwitchAfter seconds after it starts.
    // =============================================================================
    class AnimProbe : public Opaax::Behaviour
    {
    public:
        Opaax::OpaaxString SwitchTo;
        float              SwitchAfter = 1.f;

        Opaax::Int32       FramesSeen = 0;
        Opaax::Int32       LastFrame  = -1;
        Opaax::OpaaxString LastTexture;
        Opaax::OpaaxString LastSheet;
        bool               bSwitched  = false;

        OPAAX_PROPERTIES(AnimProbe, OPAAX_PROP(SwitchTo), OPAAX_PROP(SwitchAfter), OPAAX_PROP(FramesSeen),
                         OPAAX_PROP(LastFrame), OPAAX_PROP(LastTexture), OPAAX_PROP(LastSheet), OPAAX_PROP(bSwitched))

        void OnStart() override
        {
            if (!SwitchTo.IsEmpty())
            {
                SetTimer<&AnimProbe::Switch>(SwitchAfter);
            }
        }

        void OnUpdate(float) override
        {
            const Opaax::SpriteComponent* lSprite = TryGet<Opaax::SpriteComponent>();
            if (lSprite == nullptr)
            {
                return;
            }

            LastFrame   = lSprite->Frame;
            LastTexture = lSprite->Texture.Path;
            LastSheet   = lSprite->Sheet.Path;

            // Frames are counted while the first clip plays, so a switch does not add to them.
            if (!bSwitched && std::find(m_Seen.begin(), m_Seen.end(), LastFrame) == m_Seen.end())
            {
                m_Seen.push_back(LastFrame);
                FramesSeen = static_cast<Opaax::Int32>(m_Seen.size());
            }
        }

    private:
        void Switch()
        {
            if (Opaax::SpriteAnimatorComponent* lAnimator = TryGet<Opaax::SpriteAnimatorComponent>())
            {
                lAnimator->Clip = Opaax::OpaaxStringID(SwitchTo);
                bSwitched       = true;
            }
        }

        Opaax::TDynArray<Opaax::Int32> m_Seen;
    };
}
