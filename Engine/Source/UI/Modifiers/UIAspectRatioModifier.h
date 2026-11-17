#pragma once

#include "Core/Reflection/OpaaxEnum.h"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "UI/UIModifier.h"

namespace Opaax
{
    /** Which side an aspect ratio keeps (Unity's AspectRatioFitter modes). */
    enum class EUIAspectMode : Uint8
    {
        WidthControlsHeight,   // the width stays, the height follows
        HeightControlsWidth,   // the height stays, the width follows
        FitInParent,           // as large as fits inside the parent (letterboxed)
        EnvelopeParent         // as small as covers the parent (cropped by a mask)
    };

    inline const char* ToString(const EUIAspectMode InMode) noexcept
    {
        switch (InMode)
        {
        case EUIAspectMode::WidthControlsHeight: return "WidthControlsHeight";
        case EUIAspectMode::HeightControlsWidth: return "HeightControlsWidth";
        case EUIAspectMode::FitInParent:         return "FitInParent";
        case EUIAspectMode::EnvelopeParent:      return "EnvelopeParent";
        }
        return "WidthControlsHeight";
    }
}

OPAAX_ENUM_VALUES(Opaax::EUIAspectMode, WidthControlsHeight, HeightControlsWidth, FitInParent, EnvelopeParent)

namespace Opaax
{
    /**
     * The size InMode gives InSize at InRatio (width / height), inside or around InParentSize for the
     * two parent modes. A ratio of 0 or less changes nothing.
     */
    Vector2F AspectSize(EUIAspectMode InMode, float InRatio, const Vector2F& InSize, const Vector2F& InParentSize) noexcept;

    // =============================================================================
    // UIAspectRatioModifier — keeps its widget's rect at Ratio (width / height) after it resolves, about
    //   the rect's pivot: a 16:9 picture in a panel of any shape. In a stack, the two "controls" modes
    //   also shape the size it asks for.
    // =============================================================================
    class UIAspectRatioModifier final : public UIModifier
    {
        OPAAX_OBJECT(UIAspectRatioModifier, UIModifier)

    public:
        /** Width / height: 1.7778 is 16:9. 0 or less changes nothing. */
        float         Ratio = 16.f / 9.f;
        EUIAspectMode Mode  = EUIAspectMode::FitInParent;

        OPAAX_PROPERTIES(UIAspectRatioModifier,
                         OPAAX_PROP(Ratio).SetRange(0.f, 32.f).SetDragStep(0.01f)
                                          .SetTooltip("Width / height: 1.778 is 16:9, 1 is square."),
                         OPAAX_PROP(Mode).SetTooltip("WidthControlsHeight, HeightControlsWidth: one side follows the other.\n"
                                                     "FitInParent: as large as fits in the parent.\n"
                                                     "EnvelopeParent: as small as covers the parent."))

        UIAspectRatioModifier& SetRatio(float InRatio);
        UIAspectRatioModifier& SetMode(EUIAspectMode InMode);

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
