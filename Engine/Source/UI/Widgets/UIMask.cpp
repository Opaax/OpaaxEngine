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

    void UIMask::Rebuild(const UIBuildContext& InContext, TDynArray<UIQuad>& /*OutQuads*/)
    {
        // A mask emits NO quads of its own — it only resolves what its children will be cut by.
        if (Texture.IsEmpty() || InContext.Assets == nullptr)
        {
            m_Resolved = nullptr;   // an empty path is a pure rect clip, which needs no texture
            return;
        }

        m_Resolved = InContext.Assets->ResolveTexture(Texture.Path.CStr());

        if (m_Resolved == nullptr)
        {
            // Still uploading: ask again next frame rather than masking with nothing, which would
            // flash the children UNMASKED for a frame.
            InvalidateContent();
        }
    }

    void UIMask::SaveFields(nlohmann::json& InOutJson) const
    {
        UIWidget::SaveFields(InOutJson);

        InOutJson["Texture"] = Texture;
    }

    void UIMask::LoadFields(const nlohmann::json& InJson)
    {
        UIWidget::LoadFields(InJson);

        Texture = InJson.value("Texture", Texture);
    }
}
