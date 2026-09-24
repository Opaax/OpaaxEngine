#include "Editor/Resources/ResourcePreviewDrawers.h"

#include "Editor/EditorContext.h"
#include "Editor/ImguiLibrary/ImguiLayout.h"
#include "Editor/ImguiLibrary/ImguiWidgets.h"
#include "Editor/UI/IEditorUIBackend.h"

#include "Engine/Subsystems/Resources/Types/Font/FontFaceResource.h"
#include "Engine/Subsystems/Resources/Types/Texture/TextureResource.h"

#include <imgui.h>

namespace Opaax::Editor::NativeResourcePreviews
{
    namespace
    {
        /** The largest edge a preview image is drawn at, in pixels. */
        constexpr float MAX_IMAGE_SIZE = 256.f;
    }

    void DrawTexture(EditorContext& InContext, const TextureResource& InTexture)
    {
        const ImVec2 lSize = ImguiLayout::AspectFit(InTexture.Width, InTexture.Height, MAX_IMAGE_SIZE);

        ImguiWidgets::Image(InTexture.GetTexture() != nullptr
                                ? InContext.UIBackend.GetTextureImage(*InTexture.GetTexture())
                                : EditorImage{},
                            lSize);

        ImGui::TextDisabled("%u x %u", InTexture.Width, InTexture.Height);
    }

    void DrawFontFace(EditorContext& InContext, const FontFaceResource& InFace)
    {
        EditorImage lImage;

        if (InFace.GetAtlas() != nullptr)
        {
            lImage = InContext.UIBackend.GetTextureImage(*InFace.GetAtlas());

            // STRAIGHT UVs, overriding the flip GetTextureImage applies. That flip is right for a
            // TextureResource, whose pixels stb_image already turned bottom-up; a font atlas is
            // stb_truetype's raw top-down buffer, so flipping it again renders the alphabet upside
            // down. Two buffers with opposite row-0 orientations, one y-down widget.
            lImage.UV0 = { 0.f, 0.f };
            lImage.UV1 = { 1.f, 1.f };
        }

        ImguiWidgets::Image(lImage, ImguiLayout::AspectFit(InFace.Face.AtlasWidth, InFace.Face.AtlasHeight,
                                                          MAX_IMAGE_SIZE));

        ImGui::TextDisabled("%u x %u R8", InFace.Face.AtlasWidth, InFace.Face.AtlasHeight);
        ImGui::TextDisabled("%u glyph(s), %u kern pair(s)",
                            InFace.Face.GlyphCount(), static_cast<Uint32>(InFace.Face.Kerning.size()));
        ImGui::TextDisabled("baked at %.0f px", InFace.Face.PixelHeight);
        ImGui::TextDisabled("ascent %.1f  descent %.1f  line %.1f",
                            InFace.Face.VMetrics.Ascent, InFace.Face.VMetrics.Descent,
                            InFace.Face.VMetrics.LineAdvance);
    }
}
