#include "UI/Widgets/UIMask.h"

namespace Opaax
{
    void UIMask::SetTexture(const OpaaxString& InAssetPath)
    {
        Texture.Path = InAssetPath;

        // The mask is resolved by the canvas's draw walk, not by this widget's own quads — so what
        // must be rebuilt is everything UNDER it, which is what a layout invalidation reaches.
        InvalidateLayout();
    }

    void UIMask::SetShowMaskGraphic(const bool bInShow)
    {
        bShowMaskGraphic = bInShow;
        InvalidateContent();
    }

    void UIMask::Rebuild(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads)
    {
        if (Texture.IsEmpty() || InContext.Assets == nullptr)
        {
            m_Resolved = nullptr;   // an empty path is a pure rect clip, which needs no texture
        }
        else
        {
            m_Resolved = InContext.Assets->ResolveTexture(Texture.Path.CStr());

            if (m_Resolved == nullptr)
            {
                // Still uploading: ask again next frame rather than masking with nothing, which would
                // flash the children UNMASKED for a frame.
                InvalidateContent();
                return;
            }
        }

        // The graphic is its own quad, cut by itself like everything under it: a white shape on
        // transparency shows as that shape.
        if (bShowMaskGraphic)
        {
            UIQuad& lQuad = OutQuads.emplace_back();
            lQuad.Bounds  = GetBounds();
            lQuad.Texture = m_Resolved;
        }
    }

    void UIMask::SaveFields(nlohmann::json& InOutJson) const
    {
        UIWidget::SaveFields(InOutJson);

        InOutJson["Texture"]          = Texture;
        InOutJson["bShowMaskGraphic"] = bShowMaskGraphic;
    }

    void UIMask::LoadFields(const nlohmann::json& InJson)
    {
        UIWidget::LoadFields(InJson);

        Texture          = InJson.value("Texture", Texture);
        bShowMaskGraphic = InJson.value("bShowMaskGraphic", bShowMaskGraphic);
    }
}
