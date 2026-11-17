#include "UI/Modifiers/UIOutlineModifier.h"

#include "Core/Maths/Maths.h"
#include "UI/Modifiers/UIShadowModifier.h"

namespace Opaax
{
    UIOutlineModifier& UIOutlineModifier::SetDistance(const float InDistance)
    {
        Distance = Maths::Max(InDistance, 0.f);
        InvalidateOwnerContent();
        return *this;
    }

    UIOutlineModifier& UIOutlineModifier::SetColor(const LinearColor& InColor)
    {
        Color = InColor;
        InvalidateOwnerContent();
        return *this;
    }

    void UIOutlineModifier::PostEditChange()
    {
        Distance = Maths::Max(Distance, 0.f);
        InvalidateOwnerContent();
    }

    void UIOutlineModifier::OnAttach()
    {
        InvalidateOwnerContent();
    }

    void UIOutlineModifier::OnDetach()
    {
        InvalidateOwnerContent();
    }

    void UIOutlineModifier::ModifyQuads(TDynArray<UIQuad>& InOutQuads) const
    {
        if (Distance <= 0.f)
        {
            return;
        }

        const Vector2F lOffsets[] = { { -Distance, 0.f }, { Distance, 0.f }, { 0.f, Distance }, { 0.f, -Distance } };
        AppendQuadCopiesBelow(InOutQuads, lOffsets, 4, Color);
    }
}
