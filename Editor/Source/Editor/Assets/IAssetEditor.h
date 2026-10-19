#pragma once

#include "Core/Maths/MathTypes.h"   // Vector2F

namespace Opaax::Editor
{
    class AssetDocument;

    // =============================================================================
    // IAssetEditor — what an asset's window shows below its header (name, class, Save). One per open
    //   asset, built from the EditorContext. The default draws the class's properties
    //   (PropertyAssetEditor); a class registers its own with AssetTypes().Register<T>().SetEditor<E>().
    //
    //   Edit the asset directly in DrawContents: the window records the change as one undo step once
    //   no widget is active any more, after calling the asset's PostEditChange.
    // =============================================================================
    class IAssetEditor
    {
    public:
        virtual ~IAssetEditor() = default;

        /** Every frame the window is open and not collapsed. InDocument's asset is never null here. */
        virtual void DrawContents(AssetDocument& InDocument) = 0;

        /** The window's size the first time it opens. */
        virtual Vector2F GetDefaultSize() const { return { 420.f, 480.f }; }
    };
}
