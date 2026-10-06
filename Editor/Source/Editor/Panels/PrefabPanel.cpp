#include "Editor/Panels/PrefabPanel.h"

#include <imgui.h>

#include "Editor/Commands/EditorCommandRegistry.h"
#include "Editor/Commands/EditorNativeCommandsTags.hpp"
#include "Editor/EditorContext.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/ImguiLibrary/ImguiWidgets.h"
#include "Editor/Panels/EditorPanels.h"
#include "Editor/Prefab/EditorPrefabDocument.h"
#include "Editor/Extensions/DrawerRegistry.h"
#include "Editor/Operation/EditorGizmo.hpp"     // editor-wide gizmo settings
#include "Editor/Operation/EntityOps.h"         // TransformEntities
#include "Editor/Resources/ResourceDragDrop.h"  // prefab drops on the preview
#include "Editor/UI/IEditorGui.h"
#include "Editor/UI/IEditorUIBackend.h"
#include "Editor/Viewport/ViewportOverlays.h"

#include "Application/Services/IEngine.h"
#include "Core/Maths/Bounds2D.h"
#include "Resources/ResourceManager.h"   // before PrefabResource (completes LoadContext)
#include "RHI/Framebuffer.h"
#include "Renderer/CameraView.h"
#include "Renderer/RenderTarget.hpp"        // OffscreenRenderTarget

#include "World/Entity/Entity.h"
#include "World/Entity/EntityHierarchy.h"   // Detach needs a parent
#include "World/Entity/EntityMeta.h"
#include "World/Entity/EntityQuery.h"
#include "World/Prefab/PrefabResource.hpp"
#include "World/World.h"

namespace Opaax::Editor
{
    namespace
    {
        // Size used to frame an entity with nothing to draw (like EntityOps::FocusSelected).
        constexpr float k_FrameAnchor = 25.f;
    }

    PrefabPanel::~PrefabPanel() = default;

    Vector2F PrefabPanel::ViewportPx() const
    {
        return { static_cast<float>(m_Size.x), static_cast<float>(m_Size.y) };
    }

    // =========================================================================
    // OnPreRender — like the ViewportPanel: the click is used against the rendered frame, before the
    // resize and camera move.
    // =========================================================================
    void PrefabPanel::OnPreRender()
    {
        World* const lWorld = m_Context.PrefabDocument.GetWorld();

        // A hidden panel submits nothing.
        if (lWorld == nullptr || !m_Context.Panels.IsVisible(PanelID())) { return; }

        PickGesture::Apply(m_PickGesture.Take(), *lWorld, m_Context.PrefabDocument.Selection(), ViewportPx(),
                           ViewportOverlays::AnchorHalfExtent(lWorld->GetCameraView(), ViewportPx()));
        ApplyGizmoDrag(*lWorld);

        if (m_Framebuffer == nullptr)
        {
            m_Framebuffer = m_Context.Engine.CreateFramebuffer(FramebufferSpec{ m_Size.x, m_Size.y });
            if (m_Framebuffer == nullptr) { return; }

            m_RenderTarget = MakeUnique<OffscreenRenderTarget>(m_Framebuffer.get());
        }

        // Deferred resize: measured during the draw, applied before the next frame's pass.
        if (m_PendingSize.x > 0 && m_PendingSize.y > 0
            && (m_PendingSize.x != m_Size.x || m_PendingSize.y != m_Size.y))
        {
            m_Size = m_PendingSize;
            m_Framebuffer->Resize(m_Size.x, m_Size.y);
        }

        m_CameraGesture.Spend(m_Camera, ViewportPx());
        ApplyPendingFrame(*lWorld);
        m_Camera.Apply(*lWorld);   // an Edit world, so this never refuses

        // Tagged with this world, so the level's pass does not draw them (and the level grid does not land here).
        const float lAnchor = ViewportOverlays::AnchorHalfExtent(lWorld->GetCameraView(), ViewportPx());

        ViewportOverlays::EnqueueSelectionOutline(
            m_Context.Engine.GetDebugDraw(), *lWorld, m_Context.PrefabDocument.Selection().Ids(), lAnchor);
        ViewportOverlays::EnqueueEntityIcons(m_Context.Engine.GetDebugDraw(), *lWorld, lAnchor);

        // This panel's own world, framed by this panel's camera.
        m_Context.Engine.SubmitRenderView(*m_RenderTarget, lWorld->GetCameraView(), /*bInDrawOverlays*/ true, lWorld);
    }

    void PrefabPanel::ApplyPendingFrame(World& InWorld)
    {
        if (!m_bPendingFrame) { return; }
        m_bPendingFrame = false;

        TDynArray<EntityID> lIds = m_Context.PrefabDocument.Selection().Ids();
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

        m_Camera.FocusOn(lBounds, ViewportPx());   // logs where it went
    }

    void PrefabPanel::ApplyGizmoDrag(World& InWorld)
    {
        // Directly through the verb: the level command has a Play guard, but this world is always Edit.
        EntityOps::TransformDelta lDelta;
        if (m_Gizmo.TakeDelta(m_Context.Gizmo, lDelta))
        {
            EntityOps::TransformEntities(InWorld, m_Context.PrefabDocument.Selection().Ids(), lDelta);
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

        // The entities were replaced (a prefab was opened): frame them.
        if (m_Context.PrefabDocument.Generation() != m_ShownGeneration)
        {
            m_ShownGeneration = m_Context.PrefabDocument.Generation();
            m_bPendingFrame   = true;
        }

        // This window's shortcut route, ranked above EditorService's, so the level's F does not fire.
        // F is handled here because its subject is this panel's camera. Not while typing in a text field.
        if (!m_Context.Gui.IsKeyboardOwnedByUI() && ImGui::Shortcut(ImGuiKey_F))
        {
            m_bPendingFrame = true;
        }

        const bool lDirty = m_Context.PrefabDocument.IsDirty(m_Context);

        ImGui::Text("%s%s", m_Context.PrefabDocument.AbsPath().CStr(), lDirty ? " *" : "");
        ImGui::Separator();

        // The same command as Ctrl+S (the panel's SaveCommand): one save path.
        if (ImGui::Button("Save"))
        {
            m_Context.PrefabDocument.Save(m_Context);
        }

        // Always enabled: unsaved edits become the variant's overrides; the base file is not changed.
        ImGui::SameLine();
        if (ImGui::Button("Save As Variant..."))
        {
            m_Context.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_SAVE_PREFAB_AS_VARIANT, m_Context);
        }
        ImGui::SetItemTooltip("A new prefab of this one: the file, plus your edits as overrides.");

        ImGui::SameLine();
        if (ImGui::Button("Close"))
        {
            m_Context.PrefabDocument.Close();
            return;
        }

        ImGui::Separator();

        // Split horizontally: the world on the left, the rest on the right (stacked, the preview was too small).
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

        RunPendingDrop();

        if (m_bPendingDetach)
        {
            m_bPendingDetach = false;
            EntityOps::DetachSelected(m_Context, EUndoWorld::Prefab);
        }
    }

    void PrefabPanel::RunPendingDrop()
    {
        const OpaaxString lPrefab = m_PendingDropPrefab;
        m_PendingDropPrefab = OpaaxString();   // cleared first, so a refused drop does not retry

        if (lPrefab.IsEmpty()) { return; }

        World* const lWorld = m_Context.PrefabDocument.GetWorld();
        if (lWorld == nullptr) { return; }

        // This panel's camera, as published in OnPreRender (the view the drop was made in).
        const Vector2F lWorldPos = ScreenToWorld(lWorld->GetCameraView(), ViewportPx(), m_PendingDropPx);

        m_Context.PrefabDocument.Place(m_Context, lPrefab, &lWorldPos);
    }

    void PrefabPanel::DrawHierarchy()
    {
        World* const lWorld = m_Context.PrefabDocument.GetWorld();
        if (lWorld == nullptr) { return; }

        ImGui::TextDisabled("Entities");

        // The same tree as the Hierarchy, over this document's world and selection.
        m_Tree.Rebuild(*lWorld);

        for (const EntityID lRoot : m_Tree.Roots())
        {
            m_Tree.DrawNode(*lWorld, lRoot, m_Context.PrefabDocument.Selection(),
                            [this](Entity InEntity) { DrawEntityContextMenu(InEntity); });
        }

        m_Tree.DrawUnparentStrip(MapId{});

        EntityTreeDrop lDrop;
        if (m_Tree.TakeDrop(lDrop))
        {
            EntityOps::Reparent(m_Context, EUndoWorld::Prefab, lDrop.Child, lDrop.Parent);
        }

        // A prefab dropped on a row: a nested placement under it, at its authored pose.
        EntityTreePrefabDrop lPrefabDrop;
        if (m_Tree.TakePrefabDrop(lPrefabDrop))
        {
            m_Context.PrefabDocument.Place(m_Context, lPrefabDrop.AssetPath, nullptr, lPrefabDrop.OnEntity);
        }
    }

    void PrefabPanel::DrawEntityContextMenu(Entity InEntity)
    {
        if (!ImGui::BeginPopupContextItem("prefab_entity_ops")) { return; }

        // Right-clicking an unselected row selects it (like the Hierarchy).
        EditorSelection& lSelection = m_Context.PrefabDocument.Selection();
        if (!lSelection.Contains(InEntity)) { lSelection.Select(InEntity); }

        bool lHasParent = false;
        for (const EntityID lId : lSelection.Ids())
        {
            lHasParent = lHasParent || EntityHierarchy::GetParent(Entity{ lId, InEntity.GetWorld() }).IsValid();
        }

        // Queued, not run (the popup is inside the tree walk).
        if (ImGui::MenuItem("Detach from Parent", nullptr, false, lHasParent)) { m_bPendingDetach = true; }

        ImGui::EndPopup();
    }

    void PrefabPanel::DrawProperties()
    {
        Entity lEntity = m_Context.PrefabDocument.Selection().Get();

        if (!lEntity.IsValid())
        {
            ImGui::TextDisabled("Select an entity to edit it.");
            return;
        }

        // The Inspector's drawer registry, so this panel knows no component type.
        for (const TFunction<bool(Entity&, IEditorWidgets&, EditorContext&)>& lDrawer :
             m_Context.Extensions.Drawers().Entries())
        {
            lDrawer(lEntity, m_Context.Widgets, m_Context);
        }

        // A drawer writes directly, which the world cannot see; mark changes like the Inspector does.
        const bool lItemActive = ImGui::IsAnyItemActive();
        if (lItemActive || m_bWasItemActive)
        {
            if (World* lWorld = lEntity.GetWorld()) { lWorld->MarkChanged(); }
        }

        // The undo step's two edges, like the Inspector. The step goes on the document's stack.
        if (lItemActive && !m_bWasItemActive)
        {
            m_Edit.Begin(m_Context, lEntity, EUndoWorld::Prefab);
        }
        else if (!lItemActive && m_bWasItemActive)
        {
            if (m_Edit.End(m_Context)) { m_Context.PrefabDocument.Undo().Record(Move(m_Edit)); }

            m_Edit = EntityComponentsEdit{};   // closed either way, or the next edit folds into it
        }

        m_bWasItemActive = lItemActive;
    }

    void PrefabPanel::DrawPreview()
    {
        const ImVec2 lAvail = ImGui::GetContentRegionAvail();
        if (lAvail.x <= 0.f || lAvail.y <= 0.f) { return; }

        // The framebuffer is sized to the region, so the image is drawn 1:1.
        m_PendingSize = { static_cast<Uint32>(lAvail.x), static_cast<Uint32>(lAvail.y) };

        const EditorImage lImage = m_Framebuffer != nullptr
                                       ? m_Context.UIBackend.GetViewportImage(*m_Framebuffer)
                                       : EditorImage{};

        // Drawn at the framebuffer's size (it matches the region from the frame after a resize).
        const Vector2F lSizePx = ViewportPx();
        ImguiWidgets::Image(lImage, ImVec2(lSizePx.x, lSizePx.y));

        // The image is the last submitted item, so these refer to it.
        const bool   lHovered = ImGui::IsItemHovered();
        const ImVec2 lOrigin  = ImGui::GetItemRectMin();

        // Drop a prefab to nest one: stored with the pixel, run after the pass.
        {
            OpaaxString lDropped;
            if (AcceptResourceDragPayload(ResourceTypeID::Get<PrefabResource>(), lDropped))
            {
                const ImVec2 lMouse = ImGui::GetMousePos();

                m_PendingDropPrefab = lDropped;
                m_PendingDropPx     = Vector2F{ lMouse.x - lOrigin.x, lMouse.y - lOrigin.y };
            }
        }

        m_CameraGesture.Measure(lHovered, { lOrigin.x, lOrigin.y }, lSizePx);

        // One left button, two users: a press on a handle is the gizmo's, so the marquee never sees it.
        // No grid here, so translate snaps to the configured step.
        World* const lWorld = m_Context.PrefabDocument.GetWorld();
        const bool   lGizmoOwns = lWorld != nullptr
            && m_Gizmo.Measure(m_Context.Gizmo, *lWorld, m_Context.PrefabDocument.Selection(), lWorld->GetCameraView(), lSizePx,
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
    }
}
