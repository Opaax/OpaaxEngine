#include "Texture2DTypeActions.h"

#if OPAAX_WITH_EDITOR

#include <imgui.h>
#include "Assets/AssetRegistry.h"
#include "Renderer/Texture2D.h"
#include "Editor/UI/IEditorUIBackend.h"

namespace Opaax::Editor
{
    void Texture2DTypeActions::Load(OpaaxStringID InID)
    {
        AssetRegistry::Load<Texture2D>(InID);
    }

    void Texture2DTypeActions::Reload(OpaaxStringID InID)
    {
        AssetRegistry::Reload<Texture2D>(InID);
    }

    void Texture2DTypeActions::DrawPreview(OpaaxStringID InID, IEditorUIBackend& InUIBackend)
    {
        const auto lHandle = AssetRegistry::Load<Texture2D>(InID);
        if (!lHandle.IsValid()) { return; }

        const Texture2D* lTex = lHandle.Get();

        constexpr float lMaxSize = 128.f;
        const float lW      = static_cast<float>(lTex->GetWidth());
        const float lH      = static_cast<float>(lTex->GetHeight());
        const float lAspect = (lH > 0.f) ? (lW / lH) : 1.f;

        float lDrawW = lMaxSize;
        float lDrawH = lMaxSize;
        if (lAspect > 1.f) { lDrawH = lMaxSize / lAspect; }
        else               { lDrawW = lMaxSize * lAspect;  }

        // V-swap UVs (0,1)-(1,0): the texture buffer is bottom-up (stb_image's global
        // set_flip_vertically_on_load(1)) — same storage on both backends, so the UVs are
        // backend-agnostic. The ImGui handle comes from the backend seam (GL name / Vulkan set).
        const Uint64 lTexID = InUIBackend.GetTextureID(*lTex->GetRHITexture());
        if (lTexID != 0)
        {
            ImGui::Image(
                static_cast<ImTextureID>(lTexID),
                ImVec2(lDrawW, lDrawH),
                ImVec2(0.f, 1.f),
                ImVec2(1.f, 0.f)
            );
        }
        else
        {
            ImGui::Dummy(ImVec2(lDrawW, lDrawH));
        }
        ImGui::TextDisabled("%ux%u", lTex->GetWidth(), lTex->GetHeight());
    }

    bool Texture2DTypeActions::GetThumbnail(OpaaxStringID InID, IEditorUIBackend& InUIBackend, AssetThumbnail& OutThumb) const
    {
        // Only thumbnail an already-loaded texture — never force-load a whole folder.
        if (!AssetRegistry::IsLoaded(InID)) { return false; }

        const auto lHandle = AssetRegistry::Load<Texture2D>(InID);
        if (!lHandle.IsValid() || !lHandle.Get()->GetRHITexture()) { return false; }

        OutThumb.Handle = InUIBackend.GetTextureID(*lHandle.Get()->GetRHITexture());
        // V-swap (0,1)-(1,0): stb_image's global flip makes the buffer bottom-up (see DrawPreview).
        OutThumb.UV0 = { 0.f, 1.f };
        OutThumb.UV1 = { 1.f, 0.f };
        return OutThumb.Handle != 0;
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
