#include "Editor/Assets/Editors/TextureEditor.h"

#include <imgui.h>

#include "Core/Maths/Maths.h"
#include "Editor/Assets/AssetDocument.hpp"
#include "Editor/EditorContext.h"
#include "Editor/ImguiLibrary/ImguiLayout.h"
#include "Editor/ImguiLibrary/ImguiWidgets.h"
#include "Editor/UI/IEditorUIBackend.h"
#include "Renderer/Textures/Texture2D.h"

namespace Opaax::Editor
{
    namespace
    {
        /** The smallest edge the image is drawn at, however narrow the window. */
        constexpr float MIN_IMAGE_EDGE = 64.f;

        const char* PixelFormatOf(const Int32 InChannels)
        {
            switch (InChannels)
            {
                case 1:  return "R8";
                case 2:  return "RG8";
                case 3:  return "RGB8";
                case 4:  return "RGBA8";
                default: return "unknown format";
            }
        }
    }

    void TextureEditor::DrawContents(AssetDocument& InDocument)
    {
        const Texture2D* lTexture = Cast<Texture2D>(InDocument.GetAsset());
        if (lTexture == nullptr)
        {
            return;
        }

        const float  lEdge = Maths::Max(ImGui::GetContentRegionAvail().x, MIN_IMAGE_EDGE);
        const ImVec2 lSize = ImguiLayout::AspectFit(lTexture->GetWidth(), lTexture->GetHeight(), lEdge);

        ImguiWidgets::Image(lTexture->GetTexture() != nullptr
                                ? m_Context.UIBackend.GetTextureImage(*lTexture->GetTexture())
                                : EditorImage{},
                            lSize);

        ImGui::TextDisabled("%u x %u, %s", lTexture->GetWidth(), lTexture->GetHeight(),
                            PixelFormatOf(lTexture->GetChannels()));
    }
}
