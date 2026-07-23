#include "Editor/EditorPaths.h"

#include "Application/Services/ILogger.h"   // OPAAX_APP_LOG (as the base IPaths.cpp does)

#include <filesystem>

using namespace Opaax;   // OPAAX_APP_LOG expands to an unqualified ToSpdLevel(...) / LogOpaaxApplication

namespace Opaax::Editor
{
    namespace
    {
        namespace fs = std::filesystem;

        // Same convention as Engine/Source/Application/Services/IPaths.cpp: generic_string() yields '/'
        // on every OS, matching the engine's normalised path separator.
        OpaaxString ToOpaax(const fs::path& InPath)
        {
            return OpaaxString(InPath.generic_string().c_str());
        }
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
        const fs::path lRoot(EditorDir().CStr());
        return ToOpaax(lRoot / InEditorRel.CStr());
    }

    OpaaxString EditorPaths::EditorAssetToAbsolute(const OpaaxString& InAssetRel) const
    {
        const fs::path lRoot(EditorAssetsDir().CStr());
        return ToOpaax(lRoot / InAssetRel.CStr());
    }

    void EditorPaths::LogPaths() const
    {
        Paths::LogPaths();

        OPAAX_APP_LOG(Info, "OpaaxEditorApplication::Bootstrap ----> Editor Dir:          {}", EditorDir().CStr())
        OPAAX_APP_LOG(Info, "OpaaxEditorApplication::Bootstrap ----> Editor Assets Dir:   {}", EditorAssetsDir().CStr())
        OPAAX_APP_LOG(Info, "OpaaxEditorApplication::Bootstrap ----> Editor Configs Dir:  {}", EditorConfigsDir().CStr())
        OPAAX_APP_LOG(Info, "OpaaxEditorApplication::Bootstrap ----> Editor Source Dir:   {}", EditorSourceDir().CStr())
        OPAAX_APP_LOG(Info, "OpaaxEditorApplication::Bootstrap ----> Editor Save Dir:     {}", EditorSaveDir().CStr())
        OPAAX_APP_LOG(Info, "OpaaxEditorApplication::Bootstrap ----> Editor Temp Dir:     {}", EditorTempDir().CStr())
    }
}
