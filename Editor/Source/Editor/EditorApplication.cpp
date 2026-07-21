#include "Editor/EditorApplication.h"

#include "Editor/IEditorService.h"
#include "Editor/EditorService.h"
#include "Editor/EditorPaths.h"
#include "Application/Services/IEngine.h"   // Engine().Loop() — full type, not just the fwd decl
#include "Application/Services/ILogger.h"   // OPAAX_LOG + LogCategory

#include <string>

#include "Core/Maths/Angle/AngleTypes.hpp"

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
        // D1: the editor service exists ONLY in editor executables, provided at this seam.
        InServices.Provide<IEditorService, EditorService>();
        OPAAX_LOG(LogEditorApp, Info, "IEditorService provided");
    }

    void EditorApplication::PostEngineStartup()
    {
        // Engine + subsystems are up — safe to build the EditorContext now.
        GetAppService<IEditorService>().Initialize();
    }

    void EditorApplication::TickFrame()
    {
        // The editor frame wraps the engine frame in UI (S10 / D1). BeginFrame opens the ImGui frame;
        // Engine().Loop() renders the world into the backbuffer; EndFrame draws the dockspace over it and
        // submits ImGui's draw data. The host then calls Engine().Present() (S7) — the UI is on the
        // backbuffer before the swap.
        IEditorService& lEditor = GetAppService<IEditorService>();
        lEditor.BeginFrame();
        Engine().Loop();
        lEditor.EndFrame();
    }

    void EditorApplication::OnEvent(Event& InEvent)
    {
        // The editor sees window/input events FIRST — before the base app enqueues anything to the engine
        // bus (Editor.md "Event ordering", M0/S11). If the editor consumed it (S11: ImGui WantCapture*),
        // the engine never sees it. Everything else falls through to the base sink (close, resize, ...).
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
        OPAAX_LOG(LogEditorApp, Info, "Editing project '{}' -> {}", lN, lProjRel.CStr());

        return MakeUnique<EditorPaths>(InPlatform, InArgc, InArgv, lProjRel);
    }
}
