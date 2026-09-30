#include "Editor/Application/Services/EditorPaths.h"

#include "Core/Log/Logger.h"   // OPAAX_APP_LOG
#include "Core/String/OpaaxUtf8.h"

#include <filesystem>

using namespace Opaax;

namespace Opaax::Editor
{
    namespace
    {
        namespace fs = std::filesystem;
    }

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
        return Utf8::FromFsPath(Utf8::ToFsPath(EditorDir()) / Utf8::ToFsPath(InEditorRel));
    }

    OpaaxString EditorPaths::EditorAssetToAbsolute(const OpaaxString& InAssetRel) const
    {
        return Utf8::FromFsPath(Utf8::ToFsPath(EditorAssetsDir()) / Utf8::ToFsPath(InAssetRel));
    }

    // =========================================================================
    // Tool space — <WorkspaceRoot>/Editor/* (the editor's own content, independent of the project)
    // =========================================================================
    OpaaxString EditorPaths::ToolDir() const
    {
        return Utf8::FromFsPath(Utf8::ToFsPath(WorkspaceRoot()) / "Editor");
    }

    OpaaxString EditorPaths::ToolAssetsDir() const
    {
        return Utf8::FromFsPath(Utf8::ToFsPath(ToolDir()) / "Assets");
    }

    OpaaxString EditorPaths::ToolAssetToAbsolute(const OpaaxString& InAssetRel) const
    {
        return Utf8::FromFsPath(Utf8::ToFsPath(ToolAssetsDir()) / Utf8::ToFsPath(InAssetRel));
    }
}
