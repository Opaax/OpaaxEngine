#pragma once

#include "Core/Log/Logger.h"
#include "Core/Maths/MathTypes.h"   // Vector2F
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Editor/Assets/IAssetEditor.h"
#include "Editor/UI/EditorRectGeometry.h"   // ERectEdge
#include "Resources/ResourceRef.hpp"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(SpriteSheetEditor);

    class SpriteSheet;
    struct TextureResource;
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // SpriteSheetEditor — a sheet over its image: every frame outlined, the selected one highlighted. A
    //   click selects the frame under it; dragging the selected frame's middle moves it, its edge or
    //   corner resizes it, inside the texture. Below: the grid and Slice (which replaces the frames),
    //   Auto-Name, the selected frame's fields, and the frame list.
    // =============================================================================
    class SpriteSheetEditor final : public IAssetEditor
    {
    public:
        explicit SpriteSheetEditor(EditorContext& InContext) : m_Context(InContext) {}

        /** Releases the image (the window closes before the resources go). */
        ~SpriteSheetEditor() override;

        //~Begin IAssetEditor interface
        void DrawContents(AssetDocument& InDocument) override;

        Vector2F GetDefaultSize() const override { return { 520.f, 680.f }; }
        //~End IAssetEditor interface

    private:
        /** Every property but the frames and the grid (the texture, the default frame). */
        void DrawSettings(SpriteSheet& InSheet);

        /**
         * The image with the frames over it, as one ImGui item: it stays active while a frame is dragged,
         * so the window records the drag as one undo step.
         * @param InTexture The sheet's image; null shows why instead
         */
        void DrawCanvas(SpriteSheet& InSheet, const TextureResource* InTexture);

        /**
         * Selects on a click, and moves or resizes the selected frame while the canvas is held.
         * @param InTexel  The mouse, in texture pixels
         * @param InScale  Screen pixels per texture pixel
         */
        void UpdateFrameDrag(SpriteSheet& InSheet, const TextureResource& InTexture, Vector2F InTexel, float InScale,
                             bool bInClicked, bool bInHeld);

        /** The grid's fields, Slice (replaces every frame) and Auto-Name (names the unnamed). */
        void DrawSliceTools(SpriteSheet& InSheet, const TextureResource* InTexture);

        /** The selected frame's fields, and Set as Default. */
        void DrawSelectedFrame(SpriteSheet& InSheet);

        /** One row per frame: its index, name and rect; the default is marked. */
        void DrawFrameList(const SpriteSheet& InSheet);

        /** The sheet's image, claimed while the sheet names it. Null when there is none or it did not load. */
        const TextureResource* ClaimTexture(const SpriteSheet& InSheet);

        EditorContext& m_Context;

        ResourceRef<TextureResource> m_Claim;
        OpaaxString                  m_ClaimedPath;

        /** The selected frame (list and canvas), or -1. */
        Int32 m_Selected = -1;

        /** The canvas drag. None while dragging means a move, not a resize. */
        bool      m_bDraggingRect = false;
        ERectEdge m_DragEdge      = ERectEdge::None;

        /** Largest edge the image is drawn at, in pixels. */
        static constexpr float MAX_CANVAS_SIZE = 420.f;

        /** Distance to an edge, in screen pixels, that counts as grabbing it. */
        static constexpr float GRAB_THICKNESS = 5.f;
    };
}
