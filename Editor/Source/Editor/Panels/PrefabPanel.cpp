#include "Editor/Panels/PrefabPanel.h"

#include <imgui.h>

#include "Editor/EditorContext.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/ImguiLibrary/ImguiWidgets.h"
#include "Editor/Panels/EditorPanels.h"
#include "Editor/Prefab/EditorPrefabDocument.h"
#include "Editor/Extensions/DrawerRegistry.h"
#include "Editor/Operation/EditorGizmo.hpp"     // the editor-wide settings the gizmo reads
#include "Editor/Operation/EntityOps.h"         // TransformEntities — the world-explicit verb (PF12)
#include "Editor/UI/IEditorGui.h"
#include "Editor/UI/IEditorUIBackend.h"
#include "Editor/Viewport/ViewportOverlays.h"

#include "Application/Services/IEngine.h"
#include "Core/Maths/Bounds2D.h"
#include "RHI/Framebuffer.h"
#include "Renderer/CameraView.h"
#include "Renderer/RenderTarget.hpp"        // OffscreenRenderTarget

#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"
#include "World/Entity/EntityQuery.h"
#include "World/World.h"

namespace Opaax::Editor
{
    namespace
    {
        // Keeps an entity with nothing to draw framable — EntityOps::FocusSelected's value, for its
        // reason: a fixed world size, since this runs before the camera has a meaningful pixel scale.
        constexpr float k_FrameAnchor = 25.f;
    }

    PrefabPanel::~PrefabPanel() = default;

    Vector2F PrefabPanel::ViewportPx() const
    {
        return { static_cast<float>(m_Size.x), static_cast<float>(m_Size.y) };
    }

    // =========================================================================
    // OnPreRender — the ViewportPanel's order, for SEL3's reason: the click is spent against the
    // frame that was RENDERED, before the resize and the camera move that would change it.
    // =========================================================================
    void PrefabPanel::OnPreRender()
    {
        World* const lWorld = m_Context.PrefabDocument.GetWorld();

        // A hidden panel submits nothing, so it costs no pass while closed (**MV3**'s rule).
        if (lWorld == nullptr || !m_Context.Panels.IsVisible(PanelID())) { return; }

        PickGesture::Apply(m_PickGesture.Take(), *lWorld, m_Selection, ViewportPx(),
                           ViewportOverlays::AnchorHalfExtent(lWorld->GetCameraView(), ViewportPx()));
        ApplyGizmoDrag(*lWorld);

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

        m_CameraGesture.Spend(m_Camera, ViewportPx());
        ApplyPendingFrame(*lWorld);
        m_Camera.Apply(*lWorld);   // an Edit world, so this never refuses

        // Tagged with THIS world (the DebugDraw source rule), so the level's pass never sees them
        // and the level's grid never lands here.
        const float lAnchor = ViewportOverlays::AnchorHalfExtent(lWorld->GetCameraView(), ViewportPx());

        const Uint64 lOutlined = ViewportOverlays::EnqueueSelectionOutline(
            m_Context.Engine.GetDebugDraw(), *lWorld, m_Selection.Ids(), lAnchor);
        const Uint64 lIcons    = ViewportOverlays::EnqueueEntityIcons(
            m_Context.Engine.GetDebugDraw(), *lWorld, lAnchor);

        if (!m_bOutlineLogged && lOutlined > 0)
        {
            OPAAX_LOG(LogPrefabPanel, Info, "Selection outline enqueued for {} entity(ies)", lOutlined);
            m_bOutlineLogged = true;
        }
        if (!m_bIconsLogged && lIcons > 0)
        {
            OPAAX_LOG(LogPrefabPanel, Info, "Drawing {} entity icon(s) — entities with nothing to render", lIcons);
            m_bIconsLogged = true;
        }

        // NAMING ITS OWN WORLD — the whole reason P6 gave a view a Source. Framed by this panel's
        // camera, which was just published as the world's view.
        m_Context.Engine.SubmitRenderView(*m_RenderTarget, lWorld->GetCameraView(), /*bInDrawOverlays*/ true, lWorld);
    }

    void PrefabPanel::ApplyPendingFrame(World& InWorld)
    {
        if (!m_bPendingFrame) { return; }
        m_bPendingFrame = false;

        TDynArray<EntityID> lIds = m_Selection.Ids();
        if (lIds.empty())
        {
            InWorld.Each<EntityMeta>([&lIds](EntityID InId, const EntityMeta&) { lIds.emplace_back(InId); });
        }

        Bounds2D lBounds;
        if (!EntityQuery::TryGetBounds(InWorld, lIds, lBounds, k_FrameAnchor))
        {
            OPAAX_LOG(LogPrefabPanel, Warn, "Frame — nothing in the prefab has a position");
            return;
        }

        m_Camera.FocusOn(lBounds, ViewportPx());   // logs where it went, every time
    }

    void PrefabPanel::ApplyGizmoDrag(World& InWorld)
    {
        // Straight through the verb (PF12): the command the level dispatches carries the PIE
        // guard, which is the level's policy — this world is Edit whatever the level is doing.
        EntityOps::TransformDelta lDelta;
        if (m_Gizmo.TakeDelta(m_Context.Gizmo, lDelta))
        {
            EntityOps::TransformEntities(InWorld, m_Selection.Ids(), lDelta);
        }

        m_Gizmo.Close(m_Context, m_Context.PrefabDocument.Undo());
    }

    void PrefabPanel::DrawContents()
    {
        if (!m_Context.PrefabDocument.IsOpen())
        {
            ImGui::TextDisabled("No prefab open.");
            ImGui::TextDisabled("Double-click a .opaaxprefab in the Resource Browser.");
            return;
        }

        // THE ENTITIES WERE REPLACED: handles held from before may now name other entities (MV4).
        // Also the moment to frame — an opened prefab may sit anywhere in x/y.
        if (m_Context.PrefabDocument.Generation() != m_ShownGeneration)
        {
            m_ShownGeneration = m_Context.PrefabDocument.Generation();
            m_Selection.Clear();
            m_bPendingFrame = true;
        }

        // THIS WINDOW'S route, which ImGui ranks above EditorService's global one — so with this
        // panel focused the level's F and Delete do not fire. The text-field guard is the same one
        // the global route uses: typing "Fred" into a name must not frame anything.
        if (!m_Context.Gui.IsKeyboardOwnedByUI())
        {
            if (ImGui::Shortcut(ImGuiKey_F)) { m_bPendingFrame = true; }

            if (ImGui::Shortcut(ImGuiKey_Delete))
            {
                OPAAX_LOG(LogPrefabPanel, Trace, "Delete swallowed — the prefab panel has no delete verb until it has undo (P8 V4)");
            }
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
            m_Selection.Clear();
            m_Context.PrefabDocument.Close();
            return;
        }

        ImGui::Separator();

        // SPLIT HORIZONTALLY: the world on the left, everything else on the right (their call).
        // Stacked vertically the preview got whatever was left under the tree and the property
        // form, which on a docked panel is almost nothing — so the image scaled down to a stamp.
        const float lAvailX   = ImGui::GetContentRegionAvail().x;
        const float lPreviewW = lAvailX * k_PreviewSplit;

        if (ImGui::BeginChild("prefab_preview", ImVec2(lPreviewW, 0.f), ImGuiChildFlags_ResizeX))
        {
            DrawPreview();
        }
        ImGui::EndChild();

        ImGui::SameLine();

        if (ImGui::BeginChild("prefab_edit", ImVec2(0.f, 0.f)))
        {
            DrawHierarchy();
            ImGui::Separator();
            DrawProperties();
        }
        ImGui::EndChild();
    }

    void PrefabPanel::DrawHierarchy()
    {
        World* const lWorld = m_Context.PrefabDocument.GetWorld();
        if (lWorld == nullptr) { return; }

        ImGui::TextDisabled("Entities");

        lWorld->Each<EntityMeta>([this, lWorld](EntityID InId, const EntityMeta& InMeta)
        {
            ImGui::PushID(static_cast<int>(InId));

            const Entity lEntity{ InId, lWorld };

            if (ImGui::Selectable(InMeta.Name.CStr(), m_Selection.Contains(lEntity)))
            {
                if (ImGui::GetIO().KeyCtrl) { m_Selection.Toggle(lEntity); }
                else                        { m_Selection.Select(lEntity); }
            }

            ImGui::PopID();
        });
    }

    void PrefabPanel::DrawProperties()
    {
        Entity lEntity = m_Selection.Get();

        if (!lEntity.IsValid())
        {
            ImGui::TextDisabled("Select an entity to edit it.");
            return;
        }

        // THE SAME REGISTRY THE INSPECTOR USES, so a component gains a form here by being registered
        // once (**MR2i**) and this panel never learns a component type.
        for (const TFunction<bool(Entity&, IEditorWidgets&, EditorContext&)>& lDrawer :
             m_Context.Extensions.Drawers().Entries())
        {
            lDrawer(lEntity, m_Context.Widgets, m_Context);
        }

        // A drawer writes through a raw reference, which no World method sees — and IsDirty is gated
        // on the revision. The Inspector's rule: any active widget, plus the release frame, marks.
        const bool lItemActive = ImGui::IsAnyItemActive();
        if (lItemActive || m_bWasItemActive)
        {
            if (World* lWorld = lEntity.GetWorld()) { lWorld->MarkChanged(); }
        }
        m_bWasItemActive = lItemActive;
    }

    void PrefabPanel::DrawPreview()
    {
        const ImVec2 lAvail = ImGui::GetContentRegionAvail();
        if (lAvail.x <= 0.f || lAvail.y <= 0.f) { return; }

        // THE FRAMEBUFFER IS SIZED TO THE REGION, so the image is drawn 1:1 and never rescaled.
        m_PendingSize = { static_cast<Uint32>(lAvail.x), static_cast<Uint32>(lAvail.y) };

        const EditorImage lImage = m_Framebuffer != nullptr
                                       ? m_Context.UIBackend.GetViewportImage(*m_Framebuffer)
                                       : EditorImage{};

        // Drawn at the FRAMEBUFFER's size rather than the region's: they agree from the frame after
        // a resize, and using the region on the frame they disagree is exactly a stretch.
        const Vector2F lSizePx = ViewportPx();
        ImguiWidgets::Image(lImage, ImVec2(lSizePx.x, lSizePx.y));

        // The image is the LAST SUBMITTED ITEM here, so these name it (the ViewportPanel's rule).
        const bool   lHovered = ImGui::IsItemHovered();
        const ImVec2 lOrigin  = ImGui::GetItemRectMin();

        m_CameraGesture.Measure(lHovered, { lOrigin.x, lOrigin.y }, lSizePx);

        // ONE left button, TWO consumers: a press on a handle belongs to the gizmo, so the marquee
        // never sees it. No grid here, so translate snaps to the authored step.
        World* const lWorld = m_Context.PrefabDocument.GetWorld();
        const bool   lGizmoOwns = lWorld != nullptr
            && m_Gizmo.Measure(m_Context.Gizmo, *lWorld, m_Selection, lWorld->GetCameraView(), lSizePx,
                               { lOrigin.x, lOrigin.y }, lSizePx,
                               m_Context.Gizmo.GetSnapStep(EGizmoMode::Translate), /*bInSuppress*/ false,
                               EUndoWorld::Prefab);

        if (!lGizmoOwns)
        {
            m_PickGesture.Measure(lHovered, { lOrigin.x, lOrigin.y });
        }
    }

    void PrefabPanel::Shutdown()
    {
        m_RenderTarget.reset();
        m_Framebuffer.reset();

        OPAAX_LOG(LogPrefabPanel, Info, "PrefabPanel shutdown");
    }
}
