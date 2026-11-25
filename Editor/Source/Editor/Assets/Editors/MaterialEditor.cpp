#include "Editor/Assets/Editors/MaterialEditor.h"

#include <imgui.h>

#include "Core/String/OpaaxPathString.h"
#include "Editor/Assets/AssetDocument.hpp"
#include "Editor/Assets/Editors/MaterialEditing.h"
#include "Editor/Assets/PropertyAssetEditor.h"
#include "Editor/EditorContext.h"
#include "Editor/Properties/ObjectPropertyDrawer.h"
#include "Editor/UI/IEditorWidgets.h"
#include "Renderer/Materials/Material2D.h"

namespace Opaax::Editor
{
    namespace
    {
        /** The colour of what went wrong in a window. */
        const ImVec4 k_ErrorColor{ 1.f, 0.45f, 0.4f, 1.f };
    }

    void MaterialEditor::DrawContents(AssetDocument& InDocument)
    {
        Material2D* lMaterial = Cast<Material2D>(InDocument.GetAsset());
        if (lMaterial == nullptr)
        {
            PropertyAssetEditor::DrawProperties(m_Context, InDocument);
            return;
        }

        const SpriteShader*    lShader = ResolveShader(*lMaterial);
        const ShaderParamList* lParams = lShader != nullptr ? &lShader->GetParams() : nullptr;

        // A shader picked in this window asks for its defaults (not one the file already named).
        if (m_bSeenShaderField && lMaterial->Shader.Path != m_LastShaderPath && lParams != nullptr)
        {
            MaterialEditing::ApplyDefaults(*lMaterial, *lParams);
        }
        m_LastShaderPath   = lMaterial->Shader.Path;
        m_bSeenShaderField = true;

        DrawShaderSummary(*lMaterial, lShader);

        const PropertyPresentation lPresentation = [lParams](const char* InName, PropertyPresentationView& InOutView)
        {
            return MaterialEditing::Present(OpaaxStringView(InName), lParams, InOutView);
        };
        DrawObjectProperties(m_Context.Widgets, *lMaterial, lPresentation);

        // What is drawn (the viewport, a game played here, the preview) shows the values as edited.
        if (!m_bBound)
        {
            m_LiveEdit.Bind(m_Context.Resources, InDocument.GetAbsPath());
            m_bBound = true;
        }
        m_LiveEdit.Push(*lMaterial);
        m_bUnsaved = InDocument.IsDirty();
    }

    const SpriteShader* MaterialEditor::ResolveShader(const Material2D& InMaterial)
    {
        if (InMaterial.Shader.Path != m_ShaderPath)
        {
            m_ShaderPath = InMaterial.Shader.Path;
            m_Shader     = InMaterial.Shader.IsEmpty()
                               ? TAssetHandle<SpriteShader>()
                               : LoadAsset<SpriteShader>(m_Context.Resources, m_Context.Paths, InMaterial.Shader.Path);
        }
        return m_Shader.Get();
    }

    void MaterialEditor::DrawShaderSummary(const Material2D& InMaterial, const SpriteShader* InShader)
    {
        if (InMaterial.Shader.IsEmpty())
        {
            m_Context.Widgets.TextDisabled("No shader: drawn with the engine's sprite shader.");
            m_Context.Widgets.Separator();
            return;
        }

        const OpaaxString lFile(PathString::FileName(OpaaxStringView(InMaterial.Shader.Path)));
        if (InShader == nullptr)
        {
            ImGui::TextColored(k_ErrorColor, "%s did not load (see the log): drawn with the engine's sprite shader.",
                               lFile.CStr());
            m_Context.Widgets.Separator();
            return;
        }

        const ShaderParamList& lParams = InShader->GetParams();
        const OpaaxString lSummary = lParams.Params.empty()
            ? OpaaxString::Format("{} declares no @param: every value is shown.", lFile.CStr())
            : OpaaxString::Format("{} reads {} value(s), shown under its names.", lFile.CStr(),
                                  static_cast<Uint32>(lParams.Params.size()));
        m_Context.Widgets.TextDisabled(lSummary.CStr());

        for (const OpaaxString& lError : lParams.Errors)
        {
            ImGui::TextColored(k_ErrorColor, "@param %s", lError.CStr());
        }
        m_Context.Widgets.Separator();
    }
}
