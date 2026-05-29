#include "FontTypeActions.h"

#if OPAAX_WITH_EDITOR

#include <imgui.h>
#include "Assets/AssetRegistry.h"
#include "Renderer/Text/FontAsset.h"
#include "Renderer/Texture2D.h"

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

    void FontTypeActions::DrawPreview(OpaaxStringID InID)
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
        ImGui::Image(
            lAtlas->GetRendererID(),
            ImVec2(lMaxSize, lMaxSize),
            ImVec2(0.f, 0.f),
            ImVec2(1.f, 1.f)
        );

        const FontAsset::FontVMetrics& lV = lFont->GetFontVMetrics();
        ImGui::TextDisabled("Atlas: %ux%u R8", lFont->GetAtlasSize(), lFont->GetAtlasSize());
        ImGui::TextDisabled("Glyphs: %u (ASCII printable)", FontAsset::CodepointCount);
        ImGui::TextDisabled("Kern pairs: %u", lFont->GetKerningPairCount());
        ImGui::TextDisabled("VMetrics: A %.1f / D %.1f / Gap %.1f / Line %.1f",
                            lV.Ascent, lV.Descent, lV.LineGap, lV.LineAdvance);
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
