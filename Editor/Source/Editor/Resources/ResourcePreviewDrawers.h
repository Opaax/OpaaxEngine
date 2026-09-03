#pragma once

namespace Opaax
{
    struct TextureResource;
    struct FontFaceResource;
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // The editor's NATIVE resource previews — what the Preview panel shows per type, one function
    //   per previewable resource.
    //
    //   Each matches TResourcePreviewClaim<T>::FDraw, so a plain function pointer registers.
    //   EditorService::RegisterNativeResourceTypes states WHICH types have a preview; what one
    //   DRAWS is here — the NativeViewportTools shape, one route over.
    //
    //   These are CONTENTS, not chrome, so they call ImGui directly — the deliberate exception
    //   IEditorGui carves out (MR2d). Which is also why they are not lambdas in EditorService: that
    //   composition root has zero `ImGui::` and keeps it (GIZ8).
    //
    //   The resource arrives ALREADY LOADED, with a claim held for it by the live preview object.
    //   A drawer never loads anything.
    // =============================================================================
    namespace NativeResourcePreviews
    {
        /** The image itself, aspect-fit into a square box, plus its dimensions. */
        void DrawTexture(EditorContext& InContext, const TextureResource& InTexture);

        /**
         * The baked coverage atlas, plus what the bake found: glyphs, kern pairs, atlas size and the
         * scaled vertical metrics.
         *
         * The atlas draws with STRAIGHT (0,0)-(1,1) UVs while a texture needs them swapped, and the
         * asymmetry is real: stb_image flips on load so a TextureResource's row 0 is the image's
         * BOTTOM, while stb_truetype writes top-down so a font atlas's row 0 is its TOP. Two buffers,
         * opposite orientations, one y-down widget.
         */
        void DrawFontFace(EditorContext& InContext, const FontFaceResource& InFace);
    }
}
