#include "UI/Widgets/UIMask.h"

namespace Opaax
{
    void UIMask::SetTexture(const OpaaxString& InAssetPath)
    {
        Texture.Path = InAssetPath;

        // The mask applies when the children are drawn, so it is the children that must rebuild.
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
            m_Resolved = nullptr;   // empty path = rect clip, no texture
        }
        else
        {
            m_Resolved = InContext.Assets->ResolveTexture(Texture.Path.CStr());

            if (m_Resolved == nullptr)
            {
                // Still uploading: retry next frame (masking with nothing would flash the children).
                InvalidateContent();
                return;
            }
        }

        // The mask's own graphic is masked by itself (a white shape on transparency shows as that shape).
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
