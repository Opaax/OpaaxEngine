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

    void UIImage::SetTexturePath(const OpaaxString& InAssetPath)
    {
        Texture.Path = InAssetPath;
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

        // The PATH is written; the runtime pointer beside it is not, because it is a borrowed
        // handle that only code can hand over (**UI17**).
        InOutJson["Color"]      = Color;
        InOutJson["Texture"]    = Texture;
        InOutJson["Fill"]       = Fill;
        InOutJson["FillAmount"] = FillAmount;
    }

    void UIImage::LoadFields(const nlohmann::json& InJson)
    {
        UIWidget::LoadFields(InJson);

        Color      = InJson.value("Color", Color);
        Texture    = InJson.value("Texture", Texture);
        Fill       = InJson.value("Fill", Fill);
        FillAmount = InJson.value("FillAmount", FillAmount);
    }

    void UIImage::Rebuild(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads)
    {
        const float lAmount = Fill == EUIFill::None ? 1.f : std::clamp(FillAmount, 0.f, 1.f);
        if (lAmount <= 0.f)
        {
            return;
        }

        // The runtime pointer WINS; otherwise the authored path is resolved through the host.
        ITexture2D* lTexture = m_Texture;

        if (lTexture == nullptr && !Texture.IsEmpty() && InContext.Assets != nullptr)
        {
            lTexture = InContext.Assets->ResolveTexture(Texture.Path.CStr());

            if (lTexture == nullptr)
            {
                // Still uploading (or missing): draw nothing and ask again next frame (**UI3**).
                InvalidateContent();
                return;
            }
        }

        UIQuad& lQuad  = OutQuads.emplace_back();
        lQuad.Color    = Color;
        lQuad.Texture  = lTexture;

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
