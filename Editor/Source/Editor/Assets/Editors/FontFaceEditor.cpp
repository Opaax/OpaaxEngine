#include "Editor/Assets/Editors/FontFaceEditor.h"

#include <imgui.h>

#include "Core/Maths/Maths.h"
#include "Editor/Assets/AssetDocument.hpp"
#include "Editor/EditorContext.h"
#include "Editor/ImguiLibrary/ImguiLayout.h"
#include "Editor/ImguiLibrary/ImguiWidgets.h"
#include "Editor/UI/IEditorUIBackend.h"
#include "Renderer/Text/FontFace.h"

namespace Opaax::Editor
{
    namespace
    {
        /** The smallest edge the atlas is drawn at, however narrow the window. */
        constexpr float MIN_ATLAS_EDGE = 64.f;
    }

    void FontFaceEditor::DrawContents(AssetDocument& InDocument)
    {
        const FontFace* lFace = Cast<FontFace>(InDocument.GetAsset());
        if (lFace == nullptr)
        {
            return;
        }

        const FontFaceData& lData = lFace->GetData();

        EditorImage lImage;
        if (lFace->GetAtlas() != nullptr)
        {
            lImage = m_Context.UIBackend.GetTextureImage(*lFace->GetAtlas());

            // Straight UVs, overriding GetTextureImage's flip: a Texture2D is stored bottom-up, but a font
            // atlas is stb_truetype's top-down buffer, so flipping it would draw it upside down.
            lImage.UV0 = { 0.f, 0.f };
            lImage.UV1 = { 1.f, 1.f };
        }

        const float lEdge = Maths::Max(ImGui::GetContentRegionAvail().x, MIN_ATLAS_EDGE);
        ImguiWidgets::Image(lImage, ImguiLayout::AspectFit(lData.AtlasWidth, lData.AtlasHeight, lEdge));

        ImGui::TextDisabled("%u x %u R8 atlas", lData.AtlasWidth, lData.AtlasHeight);
        ImGui::TextDisabled("%u glyph(s), %u kern pair(s)", lData.GlyphCount(), static_cast<Uint32>(lData.Kerning.size()));
        ImGui::TextDisabled("baked at %.0f px", lData.PixelHeight);
        ImGui::TextDisabled("ascent %.1f  descent %.1f  line %.1f",
                            lData.VMetrics.Ascent, lData.VMetrics.Descent, lData.VMetrics.LineAdvance);
    }
}
