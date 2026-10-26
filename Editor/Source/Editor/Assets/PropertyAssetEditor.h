#pragma once

#include "Editor/Assets/IAssetEditor.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // PropertyAssetEditor — the editor every asset class gets unless it registers its own: its
    //   properties, parents' first, drawn by the same widgets as the Inspector. A property the asset
    //   hides (Object::IsPropertyVisible) is left out.
    // =============================================================================
    class PropertyAssetEditor final : public IAssetEditor
    {
    public:
        explicit PropertyAssetEditor(EditorContext& InContext) : m_Context(InContext) {}

        //~Begin IAssetEditor interface
        void DrawContents(AssetDocument& InDocument) override;
        //~End IAssetEditor interface

        /** The asset's visible properties, for a custom editor that shows them below its own view. */
        static void DrawProperties(EditorContext& InContext, AssetDocument& InDocument);

    private:
        EditorContext& m_Context;
    };
}
