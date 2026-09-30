#pragma once

#include "Core/Log/Logger.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Editor/Camera/EditorCamera.h"
#include "Editor/Panels/EntityTreeView.h"
#include "Editor/Panels/IEditorPanel.h"
#include "Editor/Undo/ComponentUndoables.h"   // EntityComponentsEdit
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
    // PrefabPanel — a prefab edited in its own world, with a real viewport.
    //   The camera and gestures belong to the panel; the world, the selection and the history belong
    //   to EditorPrefabDocument (undo steps reach documents, not panels). Nothing is shared with the
    //   level (entt reuses handles, so a shared selection could hit the wrong entity).
    //   The gestures are the level viewport's (Editor/Viewport/), so they behave the same.
    //   F is handled here (its subject is this panel's camera); Ctrl+S/Z/Y and Delete are declared on
    //   PanelDesc. Every edit goes on the document's stack. Properties use the Inspector's drawers.
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
        /** The registry key, window label and dock key. */
        OPAAX_EDITOR_PANEL_NAME(Prefab)

        // =========================================================================
        // Internal
        // =========================================================================
    private:
        /** The prefab's entities as a tree (EntityTreeView over this document's world). */
        void DrawHierarchy();

        /** A row's context menu: Detach, queued for after the walk. */
        void DrawEntityContextMenu(Entity InEntity);

        /** The primary selection's components, through the Inspector's drawer registry. */
        void DrawProperties();

        /** The rendered prefab, the deferred resize, and the gestures on the image. */
        void DrawPreview();

        /** Frames the selection, or the whole prefab. Applied in OnPreRender. */
        void ApplyPendingFrame(World& InWorld);

        /** Applies the gizmo's delta to InWorld and closes the drag on the document's stack. */
        void ApplyGizmoDrag(World& InWorld);

        /** Places the prefab dropped on the preview, if any. */
        void RunPendingDrop();

        /** The image size in pixels. */
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

        /** Measured in DrawContents, applied by OnPreRender next frame. */
        Vector2u32 m_Size        = { 512, 512 };
        Vector2u32 m_PendingSize = { 512, 512 };

        /** How much of the panel the world gets (the author can resize it). */
        static constexpr float k_PreviewSplit = 0.6f;

        // This panel's camera. The selection and history belong to the document.
        EditorCamera    m_Camera;

        CameraGesture   m_CameraGesture;
        PickGesture     m_PickGesture;
        GizmoGesture    m_Gizmo;

        /** The document generation last shown: a change means the entities were replaced (frame again). */
        Uint64          m_ShownGeneration = 0;

        /** Frame on open and on F. Set in DrawContents, applied in OnPreRender. */
        bool            m_bPendingFrame  = false;

        /** A prefab dropped on the preview this frame, and where; run after the pass. */
        OpaaxString     m_PendingDropPrefab;
        Vector2F        m_PendingDropPx = { 0.f, 0.f };

        /** The rows and their drops; Detach from the context menu, run after the pass. */
        EntityTreeView  m_Tree;
        bool            m_bPendingDetach = false;

        /** Last frame's ImGui::IsAnyItemActive (the release frame also counts). */
        bool            m_bWasItemActive = false;

        /** The property form's undo step, kept across the gesture. */
        EntityComponentsEdit m_Edit;
    };
}
