#include "Editor/Panels/PrefabPanel.h"

#include <imgui.h>

#include "Editor/EditorContext.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/ImguiLibrary/ImguiWidgets.h"
#include "Editor/Panels/EditorPanels.h"
#include "Editor/Prefab/EditorPrefabDocument.h"
#include "Editor/Extensions/DrawerRegistry.h"
#include "Editor/UI/IEditorUIBackend.h"

#include "Application/Services/IEngine.h"
#include "RHI/Framebuffer.h"
#include "Renderer/CameraView.h"
#include "Renderer/RenderTarget.hpp"        // OffscreenRenderTarget

#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"
#include "World/World.h"

namespace Opaax::Editor
{
    PrefabPanel::~PrefabPanel() = default;

    void PrefabPanel::OnPreRender()
    {
        World* const lWorld = m_Context.PrefabDocument.GetWorld();

        // A hidden panel submits nothing, so it costs no pass while closed (**MV3**'s rule).
        if (lWorld == nullptr || !m_Context.Panels.IsVisible(PanelID())) { return; }

        if (m_Framebuffer == nullptr)
        {
            m_Framebuffer = m_Context.Engine.CreateFramebuffer(FramebufferSpec{ m_Size.x, m_Size.y });
            if (m_Framebuffer == nullptr) { return; }

            m_RenderTarget = MakeUnique<OffscreenRenderTarget>(m_Framebuffer.get());
        }

        // The deferred resize ViewportPanel and the Camera Preview both use: measured during the
        // draw, applied before the next frame's pass.
        if (m_PendingSize.x > 0 && m_PendingSize.y > 0
            && (m_PendingSize.x != m_Size.x || m_PendingSize.y != m_Size.y))
        {
            m_Size = m_PendingSize;
            m_Framebuffer->Resize(m_Size.x, m_Size.y);
        }

        CameraView lView;
        lView.OrthoSize = m_OrthoSize;

        // NAMING ITS OWN WORLD — the whole reason P6 gave a view a Source. No overlays: a prefab is
        // previewed as the game will draw it, not decorated like the editor (**MV1**/**MV3**).
        m_Context.Engine.SubmitRenderView(*m_RenderTarget, lView, /*bInDrawOverlays*/ false, lWorld);
    }

    void PrefabPanel::DrawContents()
    {
        if (!m_Context.PrefabDocument.IsOpen())
        {
            ImGui::TextDisabled("No prefab open.");
            ImGui::TextDisabled("Double-click a .opaaxprefab in the Resource Browser.");
            return;
        }

        const bool lDirty = m_Context.PrefabDocument.IsDirty(m_Context);

        ImGui::Text("%s%s", m_Context.PrefabDocument.AbsPath().CStr(), lDirty ? " *" : "");
        ImGui::Separator();

        // The SAME verb Ctrl+S reaches, through the panel's SaveCommand — one save, not two
        // ([[L86]], which is exactly the bug a second hand-written save path caused).
        if (ImGui::Button("Save"))
        {
            m_Context.PrefabDocument.Save(m_Context);
        }

        ImGui::SameLine();
        if (ImGui::Button("Close"))
        {
            m_Selected = entt::null;
            m_Context.PrefabDocument.Close();
            return;
        }

        ImGui::Separator();

        DrawHierarchy();
        ImGui::Separator();
        DrawProperties();
        ImGui::Separator();
        DrawPreview();
    }

    void PrefabPanel::DrawHierarchy()
    {
        World* const lWorld = m_Context.PrefabDocument.GetWorld();
        if (lWorld == nullptr) { return; }

        ImGui::TextDisabled("Entities");

        lWorld->Each<EntityMeta>([this, lWorld](EntityID InId, const EntityMeta& InMeta)
        {
            ImGui::PushID(static_cast<int>(InId));

            if (ImGui::Selectable(InMeta.Name.CStr(), m_Selected == InId))
            {
                m_Selected = InId;
            }

            ImGui::PopID();
        });
    }

    void PrefabPanel::DrawProperties()
    {
        World* const lWorld = m_Context.PrefabDocument.GetWorld();

        if (lWorld == nullptr || m_Selected == entt::null || !lWorld->GetRegistry().valid(m_Selected))
        {
            ImGui::TextDisabled("Select an entity to edit it.");
            return;
        }

        Entity lEntity{ m_Selected, lWorld };

        // THE SAME REGISTRY THE INSPECTOR USES, so a component gains a form here by being registered
        // once (**MR2i**) and this panel never learns a component type.
        for (const TFunction<bool(Entity&, IEditorWidgets&, EditorContext&)>& lDrawer :
             m_Context.Extensions.Drawers().Entries())
        {
            lDrawer(lEntity, m_Context.Widgets, m_Context);
        }
    }

    void PrefabPanel::DrawPreview()
    {
        ImGui::TextDisabled("Preview");

        const ImVec2 lAvail = ImGui::GetContentRegionAvail();
        if (lAvail.x > 0.f && lAvail.y > 0.f)
        {
            m_PendingSize = { static_cast<Uint32>(lAvail.x), static_cast<Uint32>(lAvail.y) };
        }

        const EditorImage lImage = m_Framebuffer != nullptr
                                       ? m_Context.UIBackend.GetViewportImage(*m_Framebuffer)
                                       : EditorImage{};

        ImguiWidgets::Image(lImage, lAvail);
    }

    void PrefabPanel::Shutdown()
    {
        m_RenderTarget.reset();
        m_Framebuffer.reset();

        OPAAX_LOG(LogPrefabPanel, Info, "PrefabPanel shutdown");
    }
}
