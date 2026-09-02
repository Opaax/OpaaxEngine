#pragma once

#include "Application/Services/ILogger.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Engine/Subsystems/Resources/ResourceRef.hpp"   // the claim on the sheet's image
#include "Editor/Panels/IEditorPanel.h"
#include "Editor/UI/EditorRectGeometry.h"                // the drag's hit test and clamp
#include "Editor/Undo/SpriteSheetUndoables.h"            // the open edit gesture

namespace Opaax
{
    OPAAX_LOG_CATEGORY(SpriteSheetPanel);

    struct TextureResource;   // only NAMED by the held claim
    struct SpriteSheetData;
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // SpriteSheetPanel — the sheet EDITOR: the image with its frames drawn over it.
    //
    //   Opened by a double-click in the Resource Browser, through the seam that already answers
    //   "what does a double-click do" (ResourceTypeBuilder::SetActivate) — the same route Map,
    //   Level and the texture preview use, so this needed no new plumbing.
    //
    //   It draws EditorSpriteSheetDocument, which owns the data; the panel owns only the CLAIM on
    //   the image and the selection, both of which are presentation. That split is what lets a Save
    //   command and an undo step write the sheet without going through a panel.
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
        /** Name, dirty marker, texture and frame count — what is true of the sheet as a whole. */
        void DrawHeader(const SpriteSheetData& InData);

        /**
         * The image with every frame outlined on it, the selected one highlighted, and a click
         * selecting the frame under the cursor.
         *
         * @param InTexture The sheet's image; null draws the reason instead.
         */
        void DrawCanvas(const SpriteSheetData& InData, const TextureResource* InTexture);

        /** One selectable row per frame: its index or name, and its rect. */
        void DrawFrameList(const SpriteSheetData& InData);

        /** The grid fields and the [Slice] button that replaces the frames with what they cut. */
        void DrawSliceTools(const TextureResource* InTexture);

        /**
         * The selected frame's own fields, and the buttons acting on it.
         *
         * Bracketed for undo the way the Inspector brackets its drawers: the rising and falling
         * edges of "any item is active" open and close one step, because a TPropertyDrawer writes
         * straight through a reference and cannot report that it did.
         */
        void DrawSelectedFrame(SpriteSheetData& InData);

        /**
         * Turn a drag on the selected frame's edge, corner or middle into a new rect.
         *
         * @param InImageMin Where the image was drawn, in screen pixels.
         * @param InScale    Screen pixels per texture pixel — one number, since the fit is uniform.
         */
        void UpdateFrameDrag(SpriteSheetData& InData, const TextureResource& InTexture,
                             Vector2F InImageMin, float InScale);

        /**
         * The claim on the open sheet's image, loading it once and dropping it when the sheet
         * changes — so closing a sheet actually releases its texture.
         *
         * Null while nothing is open, when the sheet names no texture, or when it failed to load.
         */
        const TextureResource* ClaimTexture(const SpriteSheetData& InData);

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        /** Nothing to acquire — an image exists only once a sheet is opened. */
        void Startup()     override {}

        /** Nothing the world's render depends on. */
        void OnPreRender() override {}

        void DrawContents() override;

        /** Release the claim while the ResourceManager and the GL context are both alive (LC3). */
        void Shutdown()    override;

        PanelWindowStyle GetWindowStyle() const override { return { { 520.f, 560.f } }; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorContext& m_Context;

        /** The image of the sheet currently open, keyed so a re-open of the same one is free. */
        ResourceRef<TextureResource> m_Claim;
        OpaaxString                  m_ClaimedPath;

        /** Which frame the list and the canvas highlight. -1 = none. Presentation, not document state. */
        Int32 m_Selected = -1;

        // =============================================================================
        // The open edit gesture — ONE step, whichever of the two sources opened it.
        //
        //   They cannot overlap in practice: a canvas drag begins with a click on the image, which
        //   is what deactivates any field that was being typed into.
        // =============================================================================
        SheetFrameEdit m_Gesture;
        bool           m_bGestureOpen   = false;

        /** The Inspector's rising/falling edge detector, for the FIELD half of the gesture. */
        bool           m_bWasItemActive = false;

        /** The canvas drag. None WHILE DRAGGING means a move rather than a resize. */
        bool           m_bDraggingRect  = false;
        ERectEdge      m_DragEdge       = ERectEdge::None;

        /** The largest edge the sheet image is drawn at, in pixels. */
        static constexpr float MAX_CANVAS_SIZE = 420.f;

        /** How close to an edge, in SCREEN pixels, counts as grabbing it. */
        static constexpr float GRAB_THICKNESS = 5.f;
    };
}
