#include "FontTypeActions.h"

#if OPAAX_WITH_EDITOR

#include <imgui.h>
#include "Assets/AssetRegistry.h"
#include "Renderer/Text/FontAsset.h"
#include "Renderer/Texture2D.h"
#include "Editor/UI/IEditorUIBackend.h"

namespace Opaax::Editor
{
    void FontTypeActions::Load(OpaaxStringID InID)
    {
        AssetRegistry::Load<FontAsset>(InID);
    }

    void FontTypeActions::Reload(OpaaxStringID InID)
    {
        AssetRegistry::Reload<FontAsset>(InID);
    }

    void FontTypeActions::DrawPreview(OpaaxStringID InID, IEditorUIBackend& InUIBackend)
    {
        const auto lHandle = AssetRegistry::Load<FontAsset>(InID);
        if (!lHandle.IsValid()) { return; }

        const FontAsset* lFont = lHandle.Get();
        if (!lFont) { return; }

        const Texture2D* lAtlas = lFont->GetAtlasTexture();
        if (!lAtlas) { return; }

        // Atlas thumbnail. UVs are straight (0,0)-(1,1): FontAsset's atlas buffer
        // is top-down (stb_truetype writes top-down, no buffer flip), and ImGui's
        // widget is also top-down — UV(0,0) at widget top samples buffer row 0
        // = top of atlas. (Texture2DTypeActions does V-swap because stb_image's
        // global set_flip_vertically_on_load(1) makes that buffer bottom-up; that
        // convention does NOT apply here.)
        constexpr float lMaxSize = 192.f;
        const Uint64 lTexID = InUIBackend.GetTextureID(*lAtlas->GetRHITexture());
        if (lTexID != 0)
        {
            ImGui::Image(
                static_cast<ImTextureID>(lTexID),
                ImVec2(lMaxSize, lMaxSize),
                ImVec2(0.f, 0.f),
                ImVec2(1.f, 1.f)
            );
        }
        else
        {
            ImGui::Dummy(ImVec2(lMaxSize, lMaxSize));
        }

        const FontAsset::FontVMetrics& lV = lFont->GetFontVMetrics();
        ImGui::TextDisabled("Atlas: %ux%u R8", lFont->GetAtlasSize(), lFont->GetAtlasSize());
        ImGui::TextDisabled("Glyphs: %u (ASCII printable)", FontAsset::CodepointCount);
        ImGui::TextDisabled("Kern pairs: %u", lFont->GetKerningPairCount());
        ImGui::TextDisabled("VMetrics: A %.1f / D %.1f / Gap %.1f / Line %.1f",
                            lV.Ascent, lV.Descent, lV.LineGap, lV.LineAdvance);
    }

    bool FontTypeActions::GetThumbnail(OpaaxStringID InID, IEditorUIBackend& InUIBackend, AssetThumbnail& OutThumb) const
    {
        // Only thumbnail an already-loaded font — never force-load.
        if (!AssetRegistry::IsLoaded(InID)) { return false; }

        const auto lHandle = AssetRegistry::Load<FontAsset>(InID);
        if (!lHandle.IsValid()) { return false; }

        const Texture2D* lAtlas = lHandle.Get()->GetAtlasTexture();
        if (!lAtlas || !lAtlas->GetRHITexture()) { return false; }

        OutThumb.Handle = InUIBackend.GetTextureID(*lAtlas->GetRHITexture());
        // Straight (0,0)-(1,1): the atlas buffer is top-down (see DrawPreview).
        OutThumb.UV0 = { 0.f, 0.f };
        OutThumb.UV1 = { 1.f, 1.f };
        return OutThumb.Handle != 0;
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
