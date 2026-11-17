#pragma once

#include "Core/Color/LinearColor.h"
#include "Core/Color/LinearColorJson.h"
#include "UI/UIModifier.h"

namespace Opaax
{
    // =============================================================================
    // UIOutlineModifier — an outline (Unity's Outline effect): four copies of every quad of its widget,
    //   moved Distance left, right, up and down, drawn in Color under the originals. A textured quad keeps
    //   its texture, so a text gets a rim around its letters.
    // =============================================================================
    class UIOutlineModifier final : public UIModifier
    {
        OPAAX_OBJECT(UIOutlineModifier, UIModifier)

    public:
        /** Canvas units the rim reaches out. */
        float       Distance = 2.f;
        LinearColor Color    = LinearColor(0.f, 0.f, 0.f, 1.f);

        OPAAX_PROPERTIES(UIOutlineModifier,
                         OPAAX_PROP(Distance).SetRange(0.f, 64.f).SetDragStep(0.25f)
                                             .SetTooltip("How far the rim reaches out, canvas units."),
                         OPAAX_PROP(Color).SetTooltip("The rim's colour; its alpha times each quad's."))

        UIOutlineModifier& SetDistance(float InDistance);
        UIOutlineModifier& SetColor(const LinearColor& InColor);

        //~Begin Object interface
        /** An edited field rebuilds the owner's quads. */
        void PostEditChange() override;
        //~End Object interface

    protected:
        //~Begin UIModifier interface
        void OnAttach() override;
        void OnDetach() override;
        void ModifyQuads(TDynArray<UIQuad>& InOutQuads) const override;
        //~End UIModifier interface
    };
}
