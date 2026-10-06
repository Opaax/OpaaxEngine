#pragma once

#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Resources/ResourceRef.hpp"   // claim on the sheet's image
#include "Editor/Panels/IEditorPanel.h"
#include "Editor/UI/EditorRectGeometry.h"                // hit test and clamp
#include "Editor/Undo/SpriteSheetUndoables.h"            // open edit gesture

namespace Opaax
{
    OPAAX_LOG_CATEGORY(SpriteSheetPanel);

    struct TextureResource;
    struct SpriteSheetData;
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // SpriteSheetPanel — the sheet editor: the image with its frames drawn over it. Opened by a
    //   double-click in the Resource Browser. The data is in EditorSpriteSheetDocument; the panel
    //   holds the image claim and the selection.
    // =============================================================================
    class SpriteSheetPanel final : public IEditorPanel
    {
        // =============================================================================
        // Statics
        // =============================================================================
    public:
        OPAAX_EDITOR_PANEL_NAME(Sprite Sheet);

        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        explicit SpriteSheetPanel(EditorContext& InContext);
        ~SpriteSheetPanel() override;

        // =============================================================================
        // Copy delete
        // =============================================================================
        SpriteSheetPanel(const SpriteSheetPanel&)            = delete;
        SpriteSheetPanel& operator=(const SpriteSheetPanel&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /** Name, dirty marker, texture and frame count. */
        void DrawHeader(const SpriteSheetData& InData);

        /**
         * The image with every frame outlined, the selected one highlighted; a click selects the frame
         * under the cursor.
         * @param InTexture The sheet's image; null shows why instead
         */
        void DrawCanvas(const SpriteSheetData& InData, const TextureResource* InTexture);

        /** One row per frame: its index or name, and its rect. */
        void DrawFrameList(const SpriteSheetData& InData);

        /** The grid fields and the Slice button (replaces the frames). */
        void DrawSliceTools(const TextureResource* InTexture);

        /**
         * The selected frame's fields and buttons, with undo (like the Inspector).
         */
        void DrawSelectedFrame(SpriteSheetData& InData);

        /**
         * Turns a drag on the selected frame's edge, corner or middle into a new rect.
         * @param InImageMin Where the image was drawn, in screen pixels
         * @param InScale    Screen pixels per texture pixel
         */
        void UpdateFrameDrag(SpriteSheetData& InData, const TextureResource& InTexture,
                             Vector2F InImageMin, float InScale);

        /**
         * The claim on the open sheet's image, loaded once and released when the sheet changes.
         * Null when nothing is open, the sheet has no texture, or it failed to load.
         */
        const TextureResource* ClaimTexture(const SpriteSheetData& InData);

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        /** Nothing to acquire. */
        void Startup()     override {}

        /** Nothing the world render depends on. */
        void OnPreRender() override {}

        void DrawContents() override;

        /** Releases the claim while the ResourceManager and the GL context are alive. */
        void Shutdown()    override;

        PanelWindowStyle GetWindowStyle() const override { return { { 520.f, 560.f } }; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorContext& m_Context;

        /** The open sheet's image. */
        ResourceRef<TextureResource> m_Claim;
        OpaaxString                  m_ClaimedPath;

        /** Highlighted frame (list and canvas). -1 = none. */
        Int32 m_Selected = -1;

        // =============================================================================
        // The open edit gesture: one step, from the fields or from a canvas drag (they cannot overlap).
        // =============================================================================
        SheetFrameEdit m_Gesture;
        bool           m_bGestureOpen   = false;

        /** The fields' undo edge detector. */
        bool           m_bWasItemActive = false;

        /** The canvas drag. None while dragging means a move, not a resize. */
        bool           m_bDraggingRect  = false;
        ERectEdge      m_DragEdge       = ERectEdge::None;

        /** Largest edge the image is drawn at, in pixels. */
        static constexpr float MAX_CANVAS_SIZE = 420.f;

        /** Distance to an edge, in screen pixels, that counts as grabbing it. */
        static constexpr float GRAB_THICKNESS = 5.f;
    };
}
