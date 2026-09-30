#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Core/String/OpaaxStringID.hpp"
#include "UI/UIMargin.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    /** Stack direction. */
    enum class EUIAxis : Uint8
    {
        Vertical,     // top to bottom
        Horizontal    // left to right
    };

    inline const char* ToString(const EUIAxis InAxis) noexcept
    {
        return InAxis == EUIAxis::Horizontal ? "Horizontal" : "Vertical";
    }

    /** Placement across the stack's axis. Start is left (vertical) or top (horizontal). */
    enum class EUIAlign : Uint8
    {
        Start,
        Center,
        End,
        Stretch   // fills the cross axis
    };

    inline const char* ToString(const EUIAlign InAlign) noexcept
    {
        switch (InAlign)
        {
            case EUIAlign::Start:   return "Start";
            case EUIAlign::Center:  return "Center";
            case EUIAlign::End:     return "End";
            case EUIAlign::Stretch: return "Stretch";
        }
        return "Start";
    }

    // =============================================================================
    // UIStack — lays its children out one after another along an axis (like Godot's BoxContainer).
    //   It owns the children's rects: a child's Rect.SizeDelta is its desired size; anchors, pivot
    //   and position are ignored. A hidden child keeps its slot. Draws nothing, not hit-testable.
    // =============================================================================
    class OPAAX_API UIStack final : public UIWidget
    {
        // =============================================================================
        // Authored state
        // =============================================================================
    public:
        EUIAxis  Axis       = EUIAxis::Vertical;
        float    Spacing    = 0.f;     // between children, canvas units
        UIMargin Padding;              // inside my rect, canvas units
        EUIAlign ChildAlign = EUIAlign::Center;

        /** My size along the axis follows the children (only when that axis is point-anchored). */
        bool bFitContent = false;

        OPAAX_PROPERTIES(UIStack,
                         OPAAX_PROP(Axis),
                         OPAAX_PROP(Spacing).SetDragStep(1.f),
                         OPAAX_PROP(Padding).SetDragStep(1.f),
                         OPAAX_PROP(ChildAlign),
                         OPAAX_PROP(bFitContent))

        void SetAxis(EUIAxis InAxis);
        void SetSpacing(float InSpacing);
        void SetPadding(const UIMargin& InPadding);
        void SetChildAlign(EUIAlign InAlign);
        void SetFitContent(bool bInFit);

        // =============================================================================
        // UIWidget
        // =============================================================================
    public:
        UIStack();

        OpaaxStringID GetTypeName() const noexcept override { return OPAAX_ID("UIStack"); }

        void SaveFields(nlohmann::json& InOutJson) const override;
        void LoadFields(const nlohmann::json& InJson) override;

    protected:
        /** My rect, with the axis size from the content when bFitContent. */
        Bounds2D ResolveBounds(const Bounds2D& InParentBounds) const override;

        void ArrangeChildren(TDynArray<Bounds2D>& OutSlots) const override;

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        /** The children's total size along the axis, spacing and padding included. */
        float ContentExtent() const noexcept;
    };
}

OPAAX_ENUM_VALUES(Opaax::EUIAxis, Vertical, Horizontal);
OPAAX_ENUM_VALUES(Opaax::EUIAlign, Start, Center, End, Stretch);
