#include "Editor/Assets/AssetEdit.h"

#include "Editor/Assets/AssetEditors.h"
#include "Editor/EditorContext.h"

namespace Opaax::Editor
{
    void AssetEdit::Undo(EditorContext& InContext) { InContext.Assets.ApplyFromHistory(AssetPath, Before); }
    void AssetEdit::Redo(EditorContext& InContext) { InContext.Assets.ApplyFromHistory(AssetPath, After); }
}
