#include "UI/Modifiers/UISizeLimitModifier.h"

#include "Core/Maths/Maths.h"
#include "UI/UIRect.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    namespace
    {
        float LimitAxis(float InValue, const float InMin, const float InMax) noexcept
        {
            if (InMin > 0.f) { InValue = Maths::Max(InValue, InMin); }
            if (InMax > 0.f) { InValue = Maths::Min(InValue, InMax); }   // a max under the min wins
            return InValue;
        }
    }

    Vector2F LimitSize(const Vector2F& InSize, const Vector2F& InMin, const Vector2F& InMax) noexcept
    {
        return { LimitAxis(InSize.x, InMin.x, InMax.x), LimitAxis(InSize.y, InMin.y, InMax.y) };
    }

    UISizeLimitModifier& UISizeLimitModifier::SetMinSize(const Vector2F& InSize)
    {
        MinSize = InSize;
        InvalidateOwnerLayout();
        return *this;
    }

    UISizeLimitModifier& UISizeLimitModifier::SetMaxSize(const Vector2F& InSize)
    {
        MaxSize = InSize;
        InvalidateOwnerLayout();
        return *this;
    }

    void UISizeLimitModifier::PostEditChange()
    {
        InvalidateOwnerLayout();
    }

    void UISizeLimitModifier::OnAttach()
    {
        InvalidateOwnerLayout();
    }

    void UISizeLimitModifier::OnDetach()
    {
        InvalidateOwnerLayout();
    }

    void UISizeLimitModifier::ModifyBounds(Bounds2D& InOutBounds, const Bounds2D& InParent) const
    {
        (void)InParent;
        const Vector2F lPivot = GetOwner() != nullptr ? GetOwner()->Rect.Pivot : Vector2F{ 0.5f, 0.5f };
        InOutBounds = ResizeAboutPivot(InOutBounds, LimitSize(InOutBounds.Size(), MinSize, MaxSize), lPivot);
    }

    void UISizeLimitModifier::ModifyDesiredSize(Vector2F& InOutSize) const
    {
        InOutSize = LimitSize(InOutSize, MinSize, MaxSize);
    }
}
