#include "Editor/Undo/DataAssetUndoables.h"

#include "Editor/EditorContext.h"
#include "Editor/Resources/Types/DataAsset/EditorDataAssetDocument.h"

namespace Opaax::Editor
{
    namespace
    {
        void Apply(EditorContext& InContext, const OpaaxString& InPath, const nlohmann::json& InData)
        {
            EditorDataAssetDocument& lDocument = InContext.DataAssetDocument;

            if (!lDocument.IsOpen() || lDocument.AbsPath() != InPath)
            {
                OPAAX_LOG(LogEditorDataAssetDocument, Warn,
                          "Undo step skipped — its data asset '{}' is not the open one", InPath.CStr());
                return;
            }

            lDocument.SetFromJson(InData);
        }
    }

    void DataAssetEdit::Undo(EditorContext& InContext) { Apply(InContext, AssetPath, Before); }
    void DataAssetEdit::Redo(EditorContext& InContext) { Apply(InContext, AssetPath, After); }
}
