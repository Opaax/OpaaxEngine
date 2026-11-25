#include "Editor/Assets/Editors/MaterialEditor.h"

#include <imgui.h>

#include "Application/Services/IEngine.h"
#include "Core/Maths/Maths.h"
#include "Core/String/OpaaxPathString.h"
#include "Editor/Assets/AssetDocument.hpp"
#include "Editor/Assets/Editors/MaterialEditing.h"
#include "Editor/Assets/PropertyAssetEditor.h"
#include "Editor/EditorContext.h"
#include "Editor/ImguiLibrary/ImguiWidgets.h"
#include "Editor/Properties/ObjectPropertyDrawer.h"
#include "Editor/Properties/PropertyDrawers.h"
#include "Editor/UI/IEditorUIBackend.h"
#include "Editor/UI/IEditorWidgets.h"
#include "Engine/FrameClock.h"
#include "Renderer/CameraView.h"
#include "Renderer/Components/EnvironmentComponent.h"
#include "Renderer/Components/SpriteComponent.h"
#include "Renderer/Materials/Material2D.h"
#include "Renderer/RenderTarget.hpp"        // OffscreenRenderTarget
#include "Renderer/Textures/Texture2D.h"
#include "RHI/Framebuffer.h"
#include "World/Components/TransformComponent.h"
#include "World/World.h"
#include "World/WorldManager.h"

namespace Opaax::Editor
{
    namespace
    {
        /** The colour of what went wrong in a window. */
        const ImVec4 k_ErrorColor{ 1.f, 0.45f, 0.4f, 1.f };

        /** The preview square's side, in world units, and how much of the preview's height it takes. */
        constexpr float k_SquareSize   = 160.f;
        constexpr float k_SquareFill   = 0.8f;
    }

    MaterialEditor::MaterialEditor(EditorContext& InContext)
        : m_Context(InContext)
    {
        m_Context.Worlds.OnWorldDestroyed.AddMember(this, &MaterialEditor::HandleWorldDestroyed);
    }

    MaterialEditor::~MaterialEditor()
    {
        m_LiveEdit.Release(m_bUnsaved);

        m_Context.Worlds.OnWorldDestroyed.RemoveAll(this);
        if (m_PreviewWorld != nullptr)
        {
            World* const lWorld = m_PreviewWorld;
            m_PreviewWorld = nullptr;
            m_Context.Worlds.DestroyWorld(lWorld);
        }

        m_RenderTarget.reset();
        m_Framebuffer.reset();
    }

    void MaterialEditor::DrawContents(AssetDocument& InDocument)
    {
        Material2D* lMaterial = Cast<Material2D>(InDocument.GetAsset());
        if (lMaterial == nullptr)
        {
            PropertyAssetEditor::DrawProperties(m_Context, InDocument);
            return;
        }

        // What a sprite names to use this material: the preview's square does too.
        if (m_AssetPath.IsEmpty())
        {
            m_AssetPath = m_Context.Paths.AbsoluteToAsset(InDocument.GetAbsPath());
        }

        DrawPreview();

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

    // =============================================================================
    // Preview
    // =============================================================================
    void MaterialEditor::OnPreRender()
    {
        // Known once the window has been drawn: the material's path and the room it has.
        if (m_AssetPath.IsEmpty() || m_WantedWidth == 0 || !EnsurePreviewWorld())
        {
            return;
        }

        const Uint32 lHeight = static_cast<Uint32>(PREVIEW_HEIGHT);
        if (m_Framebuffer == nullptr)
        {
            m_Framebuffer = m_Context.Engine.CreateFramebuffer(FramebufferSpec{ m_WantedWidth, lHeight });
            if (m_Framebuffer == nullptr) { return; }

            m_RenderTarget  = MakeUnique<OffscreenRenderTarget>(m_Framebuffer.get());
            m_PreviewWidth  = m_WantedWidth;
            m_PreviewHeight = lHeight;
        }
        else if (m_WantedWidth != m_PreviewWidth)
        {
            m_PreviewWidth = m_WantedWidth;
            m_Framebuffer->Resize(m_PreviewWidth, m_PreviewHeight);
        }

        // The square shows this material on the chosen texture.
        SpriteComponent& lSprite = m_PreviewSquare.Get<SpriteComponent>();
        lSprite.Material.Path = m_AssetPath;
        lSprite.Texture.Path  = m_PreviewTexture;

        // Lit: an Environment makes the world HDR and lit, as in a lit level.
        if (m_bLit != m_PreviewEnvironment.IsValid())
        {
            if (m_bLit)
            {
                m_PreviewEnvironment = m_PreviewWorld->CreateEntity("Environment");
                m_PreviewEnvironment.AddOrReplace<TransformComponent>();
                m_PreviewEnvironment.Add<EnvironmentComponent>();
            }
            else
            {
                m_PreviewWorld->DestroyEntity(m_PreviewEnvironment);
                m_PreviewEnvironment = Entity();
            }
        }

        // The preview's own clock: u_Time.x moves while it runs, the app's time always does.
        if (m_bWorldTimeRuns)
        {
            m_PreviewWorld->AddStep(m_Context.Engine.GetFrameClock().GetDelta());
        }

        CameraView lView;
        lView.OrthoSize = 0.5f * k_SquareSize / k_SquareFill;
        m_Context.Engine.SubmitRenderView(*m_RenderTarget, lView, /*bInDrawOverlays*/ false, m_PreviewWorld);
    }

    bool MaterialEditor::EnsurePreviewWorld()
    {
        if (m_PreviewWorld != nullptr)
        {
            return true;
        }

        m_PreviewWorld = m_Context.Worlds.CreateWorld(OPAAX_ID("Material Preview"), EWorldMode::Edit);
        if (m_PreviewWorld == nullptr)
        {
            return false;
        }

        m_PreviewSquare = m_PreviewWorld->CreateEntity("Square");
        m_PreviewSquare.AddOrReplace<TransformComponent>();
        SpriteComponent& lSprite = m_PreviewSquare.Add<SpriteComponent>();
        lSprite.Size = Vector2F{ k_SquareSize, k_SquareSize };
        return true;
    }

    void MaterialEditor::HandleWorldDestroyed(World* InWorld)
    {
        if (InWorld != nullptr && InWorld == m_PreviewWorld)
        {
            m_PreviewWorld       = nullptr;
            m_PreviewSquare      = Entity();
            m_PreviewEnvironment = Entity();
        }
    }

    void MaterialEditor::DrawPreview()
    {
        const float lWidth = Maths::Max(ImGui::GetContentRegionAvail().x, 1.f);
        m_WantedWidth = static_cast<Uint32>(lWidth);

        // Drawn at the framebuffer's size: it matches the room from the frame after a resize.
        const EditorImage lImage = m_Framebuffer != nullptr ? m_Context.UIBackend.GetViewportImage(*m_Framebuffer)
                                                            : EditorImage{};
        ImguiWidgets::Image(lImage, ImVec2(static_cast<float>(Maths::Max(m_PreviewWidth, 1u)), PREVIEW_HEIGHT));

        m_Context.Widgets.Checkbox("World time runs", m_bWorldTimeRuns);
        m_Context.Widgets.SameLine();
        m_Context.Widgets.Checkbox("Lit", m_bLit);
        m_Context.Widgets.SameLine();
        if (m_Context.Widgets.Button("Restart time", 0.f, "The preview's world time back to 0."))
        {
            if (m_PreviewWorld != nullptr)
            {
                m_PreviewWorld->ResetSteps();
            }
        }

        DrawAssetRefField(m_Context.Widgets, "Preview texture", m_PreviewTexture, Texture2D::StaticClass());
        if (m_PreviewTexture.IsEmpty())
        {
            m_PreviewTexture = OpaaxString(DEFAULT_PREVIEW_TEXTURE);
        }
        m_Context.Widgets.Separator();
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
