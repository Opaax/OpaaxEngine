#pragma once

#include "Core/Maths/MathTypes.h"   // Vector2F
#include "Editor/Assets/IAssetEditor.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // TextureEditor — an image's window: the picture, as large as the window allows, its size and its
    //   pixel format. Nothing to edit: the image file is the source.
    // =============================================================================
    class TextureEditor final : public IAssetEditor
    {
    public:
        explicit TextureEditor(EditorContext& InContext) : m_Context(InContext) {}

        //~Begin IAssetEditor interface
        void DrawContents(AssetDocument& InDocument) override;

        Vector2F GetDefaultSize() const override { return { 420.f, 480.f }; }
        //~End IAssetEditor interface

    private:
        EditorContext& m_Context;
    };
}
