#pragma once

namespace Opaax
{
    struct FontFaceResource;
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // The editor's native resource previews: what the Preview panel shows, one function per type.
    //   Each matches TResourcePreviewClaim<T>::FDraw. EditorService::RegisterNativeResourceTypes
    //   chooses which types have a preview. Previews are contents, so they call ImGui directly.
    //   The resource arrives already loaded; a drawer never loads anything.
    // =============================================================================
    namespace NativeResourcePreviews
    {
        /**
         * The baked glyph atlas, plus what the bake found: glyphs, kern pairs, atlas size and vertical
         * metrics. Drawn with straight UVs (the atlas is top-down, unlike a texture).
         */
        void DrawFontFace(EditorContext& InContext, const FontFaceResource& InFace);
    }
}
