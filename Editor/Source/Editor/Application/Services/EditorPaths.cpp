#include "Editor/Application/Services/EditorPaths.h"

#include "Core/Log/Logger.h"   // OPAAX_APP_LOG
#include "Core/String/OpaaxPathString.h"

using namespace Opaax;

namespace Opaax::Editor
{
    // =========================================================================
    // Editor space — <ProjectRoot>/Editor/* (built with Paths::ProjectToAbsolute, recomputed on call)
    // =========================================================================
    OpaaxString EditorPaths::EditorDir()        const { return ProjectToAbsolute(OpaaxString("Editor")); }
    OpaaxString EditorPaths::EditorAssetsDir()  const { return ProjectToAbsolute(OpaaxString("Editor/Assets")); }
    OpaaxString EditorPaths::EditorConfigsDir() const { return ProjectToAbsolute(OpaaxString("Editor/Configs")); }
    OpaaxString EditorPaths::EditorSourceDir()  const { return ProjectToAbsolute(OpaaxString("Editor/Source")); }
    OpaaxString EditorPaths::EditorSaveDir()    const { return ProjectToAbsolute(OpaaxString("Editor/Save")); }
    OpaaxString EditorPaths::EditorTempDir()    const { return ProjectToAbsolute(OpaaxString("Editor/Temp")); }

    OpaaxString EditorPaths::EditorToAbsolute(const OpaaxString& InEditorRel) const
    {
        return PathString::Join(EditorDir(), InEditorRel);
    }

    OpaaxString EditorPaths::EditorAssetToAbsolute(const OpaaxString& InAssetRel) const
    {
        return PathString::Join(EditorAssetsDir(), InAssetRel);
    }

    // =========================================================================
    // Tool space — the editor's own content, independent of the project and of the workspace (a game
    // repository keeps the engine's repository in a subdirectory)
    // =========================================================================
    OpaaxString EditorPaths::ToolDir() const
    {
        return OpaaxString(OPAAX_EDITOR_DIR);
    }

    OpaaxString EditorPaths::ToolAssetsDir() const
    {
        return PathString::Join(ToolDir(), "Assets");
    }

    OpaaxString EditorPaths::ToolAssetToAbsolute(const OpaaxString& InAssetRel) const
    {
        return PathString::Join(ToolAssetsDir(), InAssetRel);
    }
}
