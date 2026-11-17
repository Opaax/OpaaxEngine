#include "UI/Modifiers/UIShadowModifier.h"

namespace Opaax
{
    void AppendQuadCopiesBelow(TDynArray<UIQuad>& InOutQuads, const Vector2F* InOffsets, const Uint64 InOffsetCount,
                               const LinearColor& InColor)
    {
        if (InOutQuads.empty() || InOffsetCount == 0)
        {
            return;
        }

        TDynArray<UIQuad> lResult;
        lResult.reserve(InOutQuads.size() * (InOffsetCount + 1));

        // Every copy of one offset, then the next: copies of the same layer stay together.
        for (Uint64 lOffset = 0; lOffset < InOffsetCount; ++lOffset)
        {
            for (const UIQuad& lQuad : InOutQuads)
            {
                UIQuad lCopy = lQuad;
                lCopy.Bounds.Center.x += InOffsets[lOffset].x;
                lCopy.Bounds.Center.y += InOffsets[lOffset].y;
                lCopy.Color = { InColor.r, InColor.g, InColor.b, InColor.a * lQuad.Color.w };
                lResult.push_back(lCopy);
            }
        }

        for (const UIQuad& lQuad : InOutQuads)
        {
            lResult.push_back(lQuad);
        }
        InOutQuads = Move(lResult);
    }

    UIShadowModifier& UIShadowModifier::SetOffset(const Vector2F& InOffset)
    {
        Offset = InOffset;
        InvalidateOwnerContent();
        return *this;
    }

    UIShadowModifier& UIShadowModifier::SetColor(const LinearColor& InColor)
    {
        Color = InColor;
        InvalidateOwnerContent();
        return *this;
    }

    void UIShadowModifier::PostEditChange()
    {
        InvalidateOwnerContent();
    }

    void UIShadowModifier::OnAttach()
    {
        InvalidateOwnerContent();
    }

    void UIShadowModifier::OnDetach()
    {
        InvalidateOwnerContent();
    }

    void UIShadowModifier::ModifyQuads(TDynArray<UIQuad>& InOutQuads) const
    {
        AppendQuadCopiesBelow(InOutQuads, &Offset, 1, Color);
    }
}
