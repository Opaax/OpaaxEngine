#include "UI/Widgets/UIText.h"

#include "Core/Reflection/OpaaxEnumJson.h"
#include "Renderer/Text/Text2D.h"
#include "UI/UIBinding.h"

namespace Opaax
{
    // =============================================================================
    // Authored state
    // =============================================================================

    void UIText::SetText(const OpaaxString& InText)
    {
        Text = InText;
        InvalidateContent();
    }

    void UIText::SetFont(const OpaaxString& InFacePath)
    {
        Font.Path = InFacePath;
        InvalidateContent();
    }

    void UIText::SetSize(const float InSize)
    {
        Size = InSize;
        InvalidateContent();
    }

    void UIText::SetColor(const LinearColor& InColor)
    {
        Color = InColor;
        InvalidateContent();
    }

    void UIText::SetAlign(const ETextAlign InHAlign, const EUIVAlign InVAlign)
    {
        HAlign = InHAlign;
        VAlign = InVAlign;
        InvalidateContent();
    }

    // =============================================================================
    // UIWidget
    // =============================================================================

    void UIText::OnPullBindings(UIBindingTable& InBindings)
    {
        if (Binding.IsEmpty())
        {
            return;
        }

        UIBoundValue lValue;
        if (!InBindings.Read(Binding, lValue))
        {
            return;   // warned once by the table; the authored text stands
        }

        OpaaxString lText = FormatBoundText(Text, lValue.ToText());
        if (m_bBound && lText == m_BoundText)
        {
            return;   // the value held: nothing to rebuild
        }

        if (!m_bBound)
        {
            OPAAX_LOG(LogUIBinding, Trace, "'{}' bound '{}' — shows \"{}\"", Name.CStr(), Binding.CStr(), lText.CStr());
        }

        m_BoundText = Move(lText);
        m_bBound    = true;
        InvalidateContent();
    }

    void UIText::SaveFields(nlohmann::json& InOutJson) const
    {
        UIWidget::SaveFields(InOutJson);

        InOutJson["Text"]            = Text;
        InOutJson["Binding"]         = Binding;
        InOutJson["Font"]            = Font;
        InOutJson["Size"]            = Size;
        InOutJson["Color"]           = Color;
        InOutJson["HAlign"]          = HAlign;
        InOutJson["VAlign"]          = VAlign;
        InOutJson["bWrap"]           = bWrap;
        InOutJson["LineHeightScale"] = LineHeightScale;
        InOutJson["bKerning"]        = bKerning;
    }

    void UIText::LoadFields(const nlohmann::json& InJson)
    {
        UIWidget::LoadFields(InJson);

        Text            = InJson.value("Text", Text);
        Binding         = InJson.value("Binding", Binding);
        Font            = InJson.value("Font", Font);
        Size            = InJson.value("Size", Size);
        Color           = InJson.value("Color", Color);
        HAlign          = InJson.value("HAlign", HAlign);
        VAlign          = InJson.value("VAlign", VAlign);
        bWrap           = InJson.value("bWrap", bWrap);
        LineHeightScale = InJson.value("LineHeightScale", LineHeightScale);
        bKerning        = InJson.value("bKerning", bKerning);
    }

    void UIText::Rebuild(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads)
    {
        const OpaaxString& lText = GetDisplayText();

        if (lText.IsEmpty() || Font.IsEmpty() || InContext.Assets == nullptr)
        {
            return;
        }

        const FontFaceView lFace = InContext.Assets->ResolveFace(Font.Path.CStr());
        if (!lFace.IsValid())
        {
            return;
        }

        // The upload is still in flight (TX4): nothing to sample yet, ask again next frame.
        if (lFace.Atlas == nullptr)
        {
            InvalidateContent();
            return;
        }

        const Bounds2D& lBounds = GetBounds();

        TextDrawParams lParams;
        lParams.Color           = Color;
        lParams.Size            = Size;
        lParams.LineHeightScale = LineHeightScale;
        lParams.bKerning        = bKerning;
        lParams.BoxWidth        = lBounds.Size().x;
        lParams.HAlign          = HAlign;
        lParams.bWrap           = bWrap;

        const Uint64 lFirst = OutQuads.size();

        // Text2D anchors at the TOP-LEFT and walks down; the rect's top-left is the origin.
        const Vector2F lExtent = Text2D::Layout(lText.CStr(), { lBounds.Min().x, lBounds.Max().y }, lFace, lParams,
            [this, &OutQuads, &lFace](const TextQuad& InQuad)
            {
                UIQuad& lQuad = OutQuads.emplace_back();
                lQuad.Bounds  = Bounds2D::FromCenterSize(InQuad.Centre, InQuad.Size);
                lQuad.Color   = Color;

                if (InQuad.bTofu)
                {
                    lQuad.Outline = Size * Text2D::TOFU_THICKNESS_RATIO;
                }
                else
                {
                    lQuad.Texture = lFace.Atlas;
                    lQuad.UVMin   = InQuad.UVMin;
                    lQuad.UVMax   = InQuad.UVMax;
                }
            });

        const float lFree  = lBounds.Size().y - lExtent.y;
        const float lShift = VAlign == EUIVAlign::Middle ? lFree * 0.5f
                           : VAlign == EUIVAlign::Bottom ? lFree : 0.f;

        if (lShift != 0.f)
        {
            for (Uint64 lIndex = lFirst; lIndex < OutQuads.size(); ++lIndex)
            {
                OutQuads[lIndex].Bounds.Center.y -= lShift;
            }
        }
    }
}
