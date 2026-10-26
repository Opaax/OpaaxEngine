#include "Editor/Assets/PropertyAssetEditor.h"

#include "Editor/Assets/AssetDocument.hpp"
#include "Editor/EditorContext.h"
#include "Editor/Properties/ObjectPropertyDrawer.h"
#include "Editor/UI/IEditorWidgets.h"

namespace Opaax::Editor
{
    void PropertyAssetEditor::DrawContents(AssetDocument& InDocument)
    {
        DrawProperties(m_Context, InDocument);
    }

    void PropertyAssetEditor::DrawProperties(EditorContext& InContext, AssetDocument& InDocument)
    {
        Asset* lAsset = InDocument.GetAsset();
        if (lAsset == nullptr)
        {
            return;
        }

        if (!DrawObjectProperties(InContext.Widgets, *lAsset))
        {
            InContext.Widgets.TextDisabled("This class has no properties.");
        }
    }
}
