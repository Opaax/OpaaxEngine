#pragma once

#include "Application/Services/ILogger.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Editor/Camera/EditorCamera.h"
#include "Editor/Panels/IEditorPanel.h"
#include "Editor/Undo/ComponentUndoables.h"   // EntityComponentsEdit — the property form's one step
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
    //   THE VIEW IS THE PANEL'S, THE REST IS THE DOCUMENT'S. The camera and the gestures are
    //   members here (view state); the world, the selection and the history live on
    //   `EditorPrefabDocument`, because an undo step replays against a selection and a step can
    //   reach a document, never a panel (P8 V4). None of it is the context's: the level's camera,
    //   selection and history must outlive a PIE cycle and be reachable by editor-wide commands,
    //   and entt REUSES handles, so a shared selection could resolve to a real but wrong entity in
    //   the other world (**MV4**, **PF9**). The gestures are the level viewport's exact types
    //   (Editor/Viewport/), so pan, zoom, click, marquee and gizmo cannot drift between surfaces.
    //
    //   `F` IS MEASURED HERE, not declared on PanelDesc like Ctrl+S/Z/Y/Delete: its subject is this
    //   panel's camera, which no command can reach. A focused window's ImGui::Shortcut takes
    //   priority over EditorService's global route, so the level's F does not also fire.
    //
    //   EVERY EDIT LANDS ON THE DOCUMENT'S STACK (P8 V3/V4): the gizmo's drag (the level's
    //   GizmoGesture, reading the editor-wide settings, applied straight through
    //   EntityOps::TransformEntities — no PIE guard, this world is Edit whatever the level does),
    //   the property form's gesture (the Inspector's bracket, verbatim), and Delete (the declared
    //   DeletePrefabSelectionCommand). Ctrl+Z reaches it through the panel's declared UndoCommand.
    //
    //   Properties come from the Inspector's drawer registry, so it never learns a component type.
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

        // THIS panel's viewpoint. The selection and the history are the DOCUMENT's (an undo step
        // replays against them, and a step can reach a document, never a panel).
        EditorCamera    m_Camera;

        CameraGesture   m_CameraGesture;
        PickGesture     m_PickGesture;
        GizmoGesture    m_Gizmo;

        /** The document generation last shown — a change means the entities were replaced: frame again. */
        Uint64          m_ShownGeneration = 0;

        /** Frame on open and on F. Set in DrawContents, spent in OnPreRender against a measured size. */
        bool            m_bPendingFrame  = false;

        /** Last frame's ImGui::IsAnyItemActive — the edit bracket's edge, and the release frame still marks. */
        bool            m_bWasItemActive = false;

        /** The property form's step, held across the gesture — the Inspector's shape (⑤). */
        EntityComponentsEdit m_Edit;

        bool            m_bOutlineLogged = false;
        bool            m_bIconsLogged   = false;
    };
}
