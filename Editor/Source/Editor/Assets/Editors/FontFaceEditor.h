#pragma once

#include "Core/Maths/MathTypes.h"   // Vector2F
#include "Editor/Assets/IAssetEditor.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // FontFaceEditor — a font file's window: its baked glyph atlas and what the bake found (glyphs,
    //   kerning pairs, atlas size, vertical metrics). Nothing to edit: the font file is the source.
    // =============================================================================
    class FontFaceEditor final : public IAssetEditor
    {
    public:
        explicit FontFaceEditor(EditorContext& InContext) : m_Context(InContext) {}

        //~Begin IAssetEditor interface
        void DrawContents(AssetDocument& InDocument) override;

        Vector2F GetDefaultSize() const override { return { 420.f, 560.f }; }
        //~End IAssetEditor interface

    private:
        EditorContext& m_Context;
    };
}
