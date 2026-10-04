#include "Editor/Operation/DataAssetOperations.h"
#include "Editor/Operation/ResourceOperations.h"

#include "Editor/EditorContext.h"
#include "Editor/Resources/Types/DataAsset/EditorDataAssetDocument.h"
#include "Editor/Undo/DataAssetUndoables.h"
#include "Editor/Undo/EditorUndo.h"

#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Engine/Subsystems/Resources/Types/DataAsset/DataAssetFile.h"
#include "Engine/Subsystems/Resources/Types/DataAsset/DataAssetResource.h"

namespace Opaax::Editor
{
    bool DataAssetOps::CommitEdit(EditorContext& InContext, const nlohmann::json& InBefore)
    {
        EditorDataAssetDocument& lDocument = InContext.DataAssetDocument;

        if (!lDocument.IsOpen() || !lDocument.IsTypeKnown()) { return false; }

        nlohmann::json lAfter = lDocument.CurrentJson();

        // A gesture that changed nothing is not a step.
        if (lAfter == InBefore)
        {
            return false;
        }

        DataAssetEdit lStep;
        lStep.AssetPath = lDocument.AbsPath();
        lStep.Before    = InBefore;
        lStep.After     = Move(lAfter);

        InContext.Undo.Record(Move(lStep));
        return true;
    }

    bool DataAssetOps::Save(EditorContext& InContext)
    {
        EditorDataAssetDocument& lDocument = InContext.DataAssetDocument;

        // An unknown type is never rewritten: it would only round-trip the raw data anyway.
        if (!lDocument.IsOpen() || !lDocument.IsTypeKnown()) { return false; }

        if (!DataAssetFile::Save(lDocument.AbsPath(), lDocument.TypeName(), lDocument.CurrentJson()))
        {
            return false;   // DataAssetFile logged why
        }

        lDocument.MarkSaved();

        ResourceOps::SavedToDisk<DataAssetResource>(InContext, lDocument.AbsPath());
        return true;
    }
}
