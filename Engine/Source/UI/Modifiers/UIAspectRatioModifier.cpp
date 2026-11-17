#include "UI/Modifiers/UIAspectRatioModifier.h"

#include "Core/Maths/Maths.h"
#include "UI/UIRect.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    Vector2F AspectSize(const EUIAspectMode InMode, const float InRatio, const Vector2F& InSize,
                        const Vector2F& InParentSize) noexcept
    {
        if (InRatio <= 0.f)
        {
            return InSize;
        }

        switch (InMode)
        {
        case EUIAspectMode::WidthControlsHeight:
            return { InSize.x, InSize.x / InRatio };

        case EUIAspectMode::HeightControlsWidth:
            return { InSize.y * InRatio, InSize.y };

        case EUIAspectMode::FitInParent:
        case EUIAspectMode::EnvelopeParent:
        {
            if (InParentSize.x <= 0.f || InParentSize.y <= 0.f)
            {
                return InSize;
            }

            // The parent's full width at the ratio, or its full height: the smaller fits, the larger covers.
            const Vector2F lByWidth  = { InParentSize.x, InParentSize.x / InRatio };
            const Vector2F lByHeight = { InParentSize.y * InRatio, InParentSize.y };
            const bool     bWidthFits = lByWidth.y <= InParentSize.y;

            if (InMode == EUIAspectMode::FitInParent)
            {
                return bWidthFits ? lByWidth : lByHeight;
            }
            return bWidthFits ? lByHeight : lByWidth;
        }
        }
        return InSize;
    }

    UIAspectRatioModifier& UIAspectRatioModifier::SetRatio(const float InRatio)
    {
        Ratio = InRatio;
        InvalidateOwnerLayout();
        return *this;
    }

    UIAspectRatioModifier& UIAspectRatioModifier::SetMode(const EUIAspectMode InMode)
    {
        Mode = InMode;
        InvalidateOwnerLayout();
        return *this;
    }

    void UIAspectRatioModifier::PostEditChange()
    {
        InvalidateOwnerLayout();
    }

    void UIAspectRatioModifier::OnAttach()
    {
        InvalidateOwnerLayout();
    }

    void UIAspectRatioModifier::OnDetach()
    {
        InvalidateOwnerLayout();
    }

    void UIAspectRatioModifier::ModifyBounds(Bounds2D& InOutBounds, const Bounds2D& InParent) const
    {
        const Vector2F lSize = AspectSize(Mode, Ratio, InOutBounds.Size(), InParent.Size());
        const Vector2F lPivot = GetOwner() != nullptr ? GetOwner()->Rect.Pivot : Vector2F{ 0.5f, 0.5f };
        InOutBounds = ResizeAboutPivot(InOutBounds, lSize, lPivot);
    }

    void UIAspectRatioModifier::ModifyDesiredSize(Vector2F& InOutSize) const
    {
        // A container gives the parent's slot: only the modes that follow a side shape the request.
        if (Mode == EUIAspectMode::WidthControlsHeight || Mode == EUIAspectMode::HeightControlsWidth)
        {
            InOutSize = AspectSize(Mode, Ratio, InOutSize, { 0.f, 0.f });
        }
    }
}
