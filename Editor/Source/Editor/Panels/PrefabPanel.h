#pragma once

#include "Application/Services/ILogger.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Editor/Camera/EditorCamera.h"
#include "Editor/Operation/EditorSelection.hpp"
#include "Editor/Panels/IEditorPanel.h"
#include "Editor/Viewport/ViewportGestures.h"
#include "Editor/Viewport/ViewportGizmo.h"

namespace Opaax
{
    class IFramebuffer;
    class OffscreenRenderTarget;
    class World;

    OPAAX_LOG_CATEGORY(PrefabPanel);
}

namespace Opaax::Editor
{
    struct EditorContext;
    struct EditorImage;

    // =============================================================================
    // PrefabPanel — a prefab edited in a world of its own (⑦-C P6), as a real viewport (P8).
    //
    //   THE THIRD VIEW, and the first that draws a world which is not the active one — **MV**'s
    //   named "asset preview WORLD" growth point, which is why P6 gave a render view a `Source`.
    //
    //   IT OWNS WHAT THE LEVEL SHARES. The level's camera, selection and gizmo live on the
    //   EditorContext because they must outlive a PIE cycle and be reachable by editor-wide
    //   commands; this panel's are its own members, because its world is never active and no
    //   command addresses it. The gestures are the level viewport's exact types (Editor/Viewport/),
    //   so pan, zoom, click and marquee cannot drift between the two surfaces.
    //
    //   ITS SELECTION IS ITS OWN, never `EditorSelection` on the context: entt REUSES handles, so a
    //   shared one could resolve to a real but wrong entity in the other world (**MV4**, **PF9**).
    //   The same hazard exists ACROSS opens — `EditorPrefabDocument::Open` clears and refills one
    //   world — so the panel clears its selection whenever the document's generation moves.
    //
    //   `F` IS MEASURED HERE, not declared on PanelDesc like Ctrl+S: its subject is this panel's
    //   camera, which no command can reach. A focused window's ImGui::Shortcut takes priority over
    //   EditorService's global route, so the level's F does not also fire. `Delete` is claimed the
    //   same way and does nothing yet — a delete with no undo is worse than none (V4 owns it), and
    //   letting it through would delete in the LEVEL.
    //
    //   THE GIZMO (P8 V3) is the level's GizmoGesture, reading the editor-wide settings (W/E/R,
    //   the toolbar) and driving its own drag. Its delta goes straight through
    //   EntityOps::TransformEntities — no PIE guard, because this world is Edit whatever the level
    //   is doing — and the drag's step lands on the DOCUMENT's stack, which Ctrl+Z reaches through
    //   the panel's declared UndoCommand. Ctrl+S's shape, twice over.
    //
    //   Properties come from the Inspector's drawer registry, so it never learns a component type.
    //   Property edits and Delete are not on the stack yet (V4).
    // =============================================================================
    class PrefabPanel final : public IEditorPanel
    {
        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        explicit PrefabPanel(EditorContext& InContext) noexcept : m_Context(InContext) {}
        ~PrefabPanel() override;

        // =========================================================================
        // Functions
        // =========================================================================
    public:
        /** The registry key, the ImGui window label and the dock key, stated once (**MR2c**). */
        OPAAX_EDITOR_PANEL_NAME(Prefab)

        // =========================================================================
        // Internal
        // =========================================================================
    private:
        /** One row per entity of the prefab; click selects, Ctrl+click toggles. */
        void DrawHierarchy();

        /** The primary's components, through the SAME drawer registry the Inspector uses. */
        void DrawProperties();

        /** The rendered prefab, the deferred resize, and the gestures measured on the image. */
        void DrawPreview();

        /** Frame the selection, or the whole prefab when nothing is selected. Spent in OnPreRender. */
        void ApplyPendingFrame(World& InWorld);

        /** Spend the gizmo's banked delta on InWorld and close the drag onto the document's stack. */
        void ApplyGizmoDrag(World& InWorld);

        /** The image's size in pixels, as a float pair — what every conversion takes. */
        Vector2F ViewportPx() const;

        // =========================================================================
        // Override
        // =========================================================================
        //~Begin IEditorPanel interface
    public:
        void            Startup()     override {}
        void            OnPreRender() override;
        void            DrawContents() override;
        void            Shutdown()    override;
        //~End IEditorPanel interface

        // =========================================================================
        // Members
        // =========================================================================
    private:
        EditorContext& m_Context;

        TUniquePtr<IFramebuffer>          m_Framebuffer;
        TUniquePtr<OffscreenRenderTarget> m_RenderTarget;

        /** Measured in DrawContents, applied by OnPreRender next frame — the deferred resize. */
        Vector2u32 m_Size        = { 512, 512 };
        Vector2u32 m_PendingSize = { 512, 512 };

        /** How much of the panel the world gets. ImGui's ResizeX lets the author move it from there. */
        static constexpr float k_PreviewSplit = 0.6f;

        // THIS panel's viewpoint and selection — see the class note on why neither is the context's.
        EditorCamera    m_Camera;
        EditorSelection m_Selection;

        CameraGesture   m_CameraGesture;
        PickGesture     m_PickGesture;
        GizmoGesture    m_Gizmo;

        /** The document generation last shown — a change means the entities were replaced (see the note). */
        Uint64          m_ShownGeneration = 0;

        /** Frame on open and on F. Set in DrawContents, spent in OnPreRender against a measured size. */
        bool            m_bPendingFrame  = false;

        /** Last frame's ImGui::IsAnyItemActive — the release frame of a widget still marks the world. */
        bool            m_bWasItemActive = false;

        bool            m_bOutlineLogged = false;
        bool            m_bIconsLogged   = false;
    };
}
