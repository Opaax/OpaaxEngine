#pragma once

#include "Core/Maths/MathsJson.hpp"
#include "UI/UIModifier.h"

namespace Opaax
{
    /** InSize kept between InMin and InMax, each axis alone; 0 on an axis means no limit there. */
    Vector2F LimitSize(const Vector2F& InSize, const Vector2F& InMin, const Vector2F& InMax) noexcept;

    // =============================================================================
    // UISizeLimitModifier — keeps its widget's size between MinSize and MaxSize after its rect resolves
    //   (about the rect's pivot), and in what it asks a stack for, so the stack reserves the limited
    //   size (Unity's LayoutElement min and max). 0 on an axis: no limit there.
    // =============================================================================
    class UISizeLimitModifier final : public UIModifier
    {
        OPAAX_OBJECT(UISizeLimitModifier, UIModifier)

    public:
        /** Canvas units; 0 on an axis: no minimum there. */
        Vector2F MinSize = { 0.f, 0.f };
        /** Canvas units; 0 on an axis: no maximum there. */
        Vector2F MaxSize = { 0.f, 0.f };

        OPAAX_PROPERTIES(UISizeLimitModifier,
                         OPAAX_PROP(MinSize).SetRange(0.f, 16384.f).SetDragStep(1.f)
                                            .SetTooltip("The smallest it gets, canvas units. 0 on an axis: no minimum."),
                         OPAAX_PROP(MaxSize).SetRange(0.f, 16384.f).SetDragStep(1.f)
                                            .SetTooltip("The largest it gets, canvas units. 0 on an axis: no maximum."))

        UISizeLimitModifier& SetMinSize(const Vector2F& InSize);
        UISizeLimitModifier& SetMaxSize(const Vector2F& InSize);

        //~Begin Object interface
        /** An edited field lays the owner out again. */
        void PostEditChange() override;
        //~End Object interface

    protected:
        //~Begin UIModifier interface
        void OnAttach() override;
        void OnDetach() override;
        void ModifyBounds(Bounds2D& InOutBounds, const Bounds2D& InParent) const override;
        void ModifyDesiredSize(Vector2F& InOutSize) const override;
        //~End UIModifier interface
    };
}
