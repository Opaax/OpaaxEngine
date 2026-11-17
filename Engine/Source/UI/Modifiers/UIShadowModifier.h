#pragma once

#include "Core/Color/LinearColor.h"
#include "Core/Color/LinearColorJson.h"
#include "Core/Maths/MathsJson.hpp"
#include "UI/UIModifier.h"

namespace Opaax
{
    // =============================================================================
    // UIShadowModifier — a drop shadow (Unity's Shadow effect): a copy of every quad of its widget,
    //   moved by Offset and drawn in Color under the originals. A textured quad keeps its texture,
    //   so a text's shadow has its letters. Each copy's alpha is Color's times its quad's.
    // =============================================================================
    class UIShadowModifier final : public UIModifier
    {
        OPAAX_OBJECT(UIShadowModifier, UIModifier)

    public:
        /** Canvas units, Y up: { 4, -4 } is down and right. */
        Vector2F    Offset = { 4.f, -4.f };
        LinearColor Color  = LinearColor(0.f, 0.f, 0.f, 0.5f);

        OPAAX_PROPERTIES(UIShadowModifier,
                         OPAAX_PROP(Offset).SetDragStep(0.5f)
                                           .SetTooltip("Where the shadow falls, canvas units: 4, -4 is down and right."),
                         OPAAX_PROP(Color).SetTooltip("The shadow's colour; its alpha times each quad's."))

        UIShadowModifier& SetOffset(const Vector2F& InOffset);
        UIShadowModifier& SetColor(const LinearColor& InColor);

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

    /**
     * InQuads with a copy of each, moved by InOffset and coloured InColor (alpha times the quad's), the
     * copies first so they draw under. Pure: what UIShadowModifier and UIOutlineModifier do.
     */
    void AppendQuadCopiesBelow(TDynArray<UIQuad>& InOutQuads, const Vector2F* InOffsets, Uint64 InOffsetCount,
                               const LinearColor& InColor);
}
