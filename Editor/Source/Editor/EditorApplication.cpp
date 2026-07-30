#include "Editor/EditorApplication.h"

#include "Editor/IEditorService.h"
#include "Editor/EditorService.h"
#include "Editor/EditorPaths.h"

#include "Application/Services/IEngine.h"   // Engine().Loop() — full type, not just the fwd decl
#include "Application/Services/ILogger.h"   // OPAAX_LOG + LogCategory

#include <string>

using namespace Opaax;   // OPAAX_LOG expands to an unqualified ToSpdLevel(...)

namespace
{
    constexpr LogCategory LogEditorApp{"EditorApplication"};
}

namespace Opaax::Editor
{
    EditorApplication::EditorApplication(int InArgc, char** InArgv)
        : OpaaxApplication(InArgc, InArgv)
    {
    }

    void EditorApplication::OnProvideServices(AppServiceLocator& InServices)
    {
        InServices.Provide<IEditorService, EditorService>();
        OPAAX_LOG(LogEditorApp, Info, "IEditorService provided");
    }

    WorldSpec EditorApplication::GetStartupWorldSpec() const
    {
        // Same world the runtime would open, but for authoring. Only the mode differs, so the
        // base keeps owning where the name comes from.
        WorldSpec lSpec = OpaaxApplication::GetStartupWorldSpec();
        lSpec.Mode      = EWorldMode::Edit;

        return lSpec;
    }

    void EditorApplication::PostEngineStartup()
    {
        GetAppService<IEditorService>().Initialize();
    }

    void EditorApplication::TickFrame()
    {
        IEditorService& lEditor = GetAppService<IEditorService>();
        lEditor.BeginFrame();
        Engine().Loop();
        lEditor.EndFrame();
    }

    void EditorApplication::OnEvent(Event& InEvent)
    {
        if (GetAppService<IEditorService>().RouteInput(InEvent))
        {
            return;
        }

        OpaaxApplication::OnEvent(InEvent);
    }

    void EditorApplication::OnModulesRegistered()
    {
        GetAppService<IEditorService>().RegisterExtensions([this](EditorExtensionRegistrar& InRegistrar) { OnRegisterEditorModules(InRegistrar); });
    }

    UniquePtr<IPaths> EditorApplication::CreatePaths(const IPlatform& InPlatform, int InArgc, char** InArgv)
    {
        const OpaaxString lName = GetEditedProjectName();
        if (lName.IsEmpty())
        {
            // No project declared — fall back to the exe-stem default (usually wrong for an editor exe).
            OPAAX_LOG(LogEditorApp, Warn,
                "No edited project declared (override GetEditedProjectName); using the exe-stem default.");
            return OpaaxApplication::CreatePaths(InPlatform, InArgc, InArgv);
        }

        // Layout convention: <name>/<name>.opaaxproj under the source workspace.
        const std::string lN      = lName.CStr();
        const OpaaxString  lProjRel((lN + "/" + lN + ".opaaxproj").c_str());
        OPAAX_LOG(LogEditorApp, Info, "Editing project '{}' -> {}", lN, lProjRel.CStr())

        return MakeUnique<EditorPaths>(InPlatform, InArgc, InArgv, lProjRel);
    }
}
