#include "Editor/Application/Services/EditorPaths.h"

#include "Application/Services/ILogger.h"   // OPAAX_APP_LOG (as the base IPaths.cpp does)
#include "Core/String/OpaaxUtf8.h"         // I7 — shared with IPaths.cpp, no local copy

#include <filesystem>

using namespace Opaax;   // OPAAX_APP_LOG expands to an unqualified ToSpdLevel(...) / LogOpaaxApplication

namespace Opaax::Editor
{
    namespace
    {
        namespace fs = std::filesystem;
    }

    // =========================================================================
    // Editor space — <ProjectRoot>/Editor/*  (derived from the inherited ProjectRoot)
    //   The named dirs compose over Paths::ProjectToAbsolute, which already does the fs::path join +
    //   generic_string() normalisation — so this reuses the exact convention the base uses.
    //   Recomputed on call (never a hot path); no cached EditorLayout state (Simple > clever).
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
    // Tool space — <WorkspaceRoot>/Editor/*  (the editor binary's own content)
    //   Anchored on WorkspaceRoot rather than ProjectRoot, so it answers the same thing whichever
    //   project is open — or none at all.
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

    void EditorPaths::LogPaths() const
    {
        Paths::LogPaths();

        OPAAX_APP_LOG(Info, "OpaaxEditorApplication::Bootstrap ----> Editor Dir:          {}", EditorDir().CStr());
        OPAAX_APP_LOG(Info, "OpaaxEditorApplication::Bootstrap ----> Editor Assets Dir:   {}", EditorAssetsDir().CStr());
        OPAAX_APP_LOG(Info, "OpaaxEditorApplication::Bootstrap ----> Editor Configs Dir:  {}", EditorConfigsDir().CStr());
        OPAAX_APP_LOG(Info, "OpaaxEditorApplication::Bootstrap ----> Editor Source Dir:   {}", EditorSourceDir().CStr());
        OPAAX_APP_LOG(Info, "OpaaxEditorApplication::Bootstrap ----> Editor Save Dir:     {}", EditorSaveDir().CStr());
        OPAAX_APP_LOG(Info, "OpaaxEditorApplication::Bootstrap ----> Editor Temp Dir:     {}", EditorTempDir().CStr());
        OPAAX_APP_LOG(Info, "OpaaxEditorApplication::Bootstrap ----> Tool Assets Dir:     {}", ToolAssetsDir().CStr());
    }
}
