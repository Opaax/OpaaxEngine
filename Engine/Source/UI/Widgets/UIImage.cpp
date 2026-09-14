#include "UI/Widgets/UIImage.h"

#include <algorithm>

#include "Core/Reflection/OpaaxEnumJson.h"

namespace Opaax
{
    void UIImage::SetColor(const LinearColor& InColor)
    {
        Color = InColor;
        InvalidateContent();
    }

    void UIImage::SetFill(const EUIFill InFill, const float InAmount)
    {
        Fill       = InFill;
        FillAmount = InAmount;
        InvalidateContent();
    }

    void UIImage::SetFillAmount(const float InAmount)
    {
        FillAmount = InAmount;
        InvalidateContent();
    }

    void UIImage::SetTexture(ITexture2D* InTexture)
    {
        m_Texture = InTexture;
        InvalidateContent();
    }

    void UIImage::SaveFields(nlohmann::json& InOutJson) const
    {
        UIWidget::SaveFields(InOutJson);

        // Texture is NOT written: it is a borrowed runtime pointer. A texture PATH field arrives
        // with the sliced image (U5), which is when a UI image first names an asset.
        InOutJson["Color"]      = Color;
        InOutJson["Fill"]       = Fill;
        InOutJson["FillAmount"] = FillAmount;
    }

    void UIImage::LoadFields(const nlohmann::json& InJson)
    {
        UIWidget::LoadFields(InJson);

        Color      = InJson.value("Color", Color);
        Fill       = InJson.value("Fill", Fill);
        FillAmount = InJson.value("FillAmount", FillAmount);
    }

    void UIImage::Rebuild(const UIBuildContext& /*InContext*/, TDynArray<UIQuad>& OutQuads)
    {
        const float lAmount = Fill == EUIFill::None ? 1.f : std::clamp(FillAmount, 0.f, 1.f);
        if (lAmount <= 0.f)
        {
            return;
        }

        UIQuad& lQuad  = OutQuads.emplace_back();
        lQuad.Color    = Color;
        lQuad.Texture  = m_Texture;

        const Vector2F lMin  = GetBounds().Min();
        Vector2F       lSize = GetBounds().Size();

        // Crop from the min edge so the bar empties toward it; the UVs crop the same fraction so
        // the texture is cut, not squashed.
        if (Fill == EUIFill::Horizontal)
        {
            lSize.x      *= lAmount;
            lQuad.UVMax.x = lAmount;
        }
        else if (Fill == EUIFill::Vertical)
        {
            lSize.y      *= lAmount;
            lQuad.UVMax.y = lAmount;
        }

        lQuad.Bounds = Bounds2D::FromCenterSize(lMin + lSize * 0.5f, lSize);
    }
}
