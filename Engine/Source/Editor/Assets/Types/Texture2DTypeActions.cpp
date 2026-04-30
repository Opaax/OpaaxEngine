#include "Texture2DTypeActions.h"

#if OPAAX_WITH_EDITOR

#include <imgui.h>
#include "Assets/AssetRegistry.h"
#include "RHI/OpenGL/OpenGLTexture2D.h"

namespace Opaax::Editor
{
    void Texture2DTypeActions::Load(OpaaxStringID InID)
    {
        AssetRegistry::Load<OpenGLTexture2D>(InID);
    }

    void Texture2DTypeActions::Reload(OpaaxStringID InID)
    {
        AssetRegistry::Reload<OpenGLTexture2D>(InID);
    }

    void Texture2DTypeActions::DrawPreview(OpaaxStringID InID)
    {
        const auto lHandle = AssetRegistry::Load<OpenGLTexture2D>(InID);
        if (!lHandle.IsValid()) { return; }

        const OpenGLTexture2D* lTex = lHandle.Get();

        constexpr float lMaxSize = 128.f;
        const float lW      = static_cast<float>(lTex->GetWidth());
        const float lH      = static_cast<float>(lTex->GetHeight());
        const float lAspect = (lH > 0.f) ? (lW / lH) : 1.f;

        float lDrawW = lMaxSize;
        float lDrawH = lMaxSize;
        if (lAspect > 1.f) { lDrawH = lMaxSize / lAspect; }
        else               { lDrawW = lMaxSize * lAspect;  }

        ImGui::Image(
            lTex->GetRendererID(),
            ImVec2(lDrawW, lDrawH),
            ImVec2(0.f, 1.f),
            ImVec2(1.f, 0.f)
        );
        ImGui::TextDisabled("%ux%u", lTex->GetWidth(), lTex->GetHeight());
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
