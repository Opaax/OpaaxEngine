

#include "EditorApplication.h"

#include "Editor/Application/Services/EditorService.h"
#include "Editor/Application/Services/EditorPaths.h"
#include "Editor/Imgui/Configs/Config_EditorImgui.h"
#include "Editor/Panels/LogPanel.h"   // MAX_LINES — the history keeps what the panel can show

#include "Application/Services/IConfigSystem.h"

#include "Application/Services/IEngine.h"   // Engine().Loop() — full type, not just the fwd decl
#include "Core/Log/Logger.h"   // OPAAX_LOG + LogCategory

#include <string>

using namespace Opaax;

namespace
{
    constexpr LogCategory LogEditorApp{"EditorApplication"};
}

namespace Opaax::Editor
{
    // =============================================================================
    // Ctor
    // =============================================================================
    
    EditorApplication::EditorApplication(int InArgc, char** InArgv)
        : OpaaxApplication(InArgc, InArgv)
    {
        // Here, not in EditorService: this runs before Bootstrap, so the Log panel gets the boot lines.
        // A game never calls it, so the history costs a shipped build nothing (block LG).
        Logger::Get().EnableHistory(LogPanel::MAX_LINES);
    }
    
    // =============================================================================
    // Native Editor App
    // =============================================================================

    // =============================================================================
    // Getters
    // =============================================================================
    
    IEditorService& EditorApplication::Editor()
    {
        return GetAppService<IEditorService>();
    }

    // =============================================================================
    // Overrides
    // =============================================================================
    
    void EditorApplication::OnProvideServices(AppServiceLocator& InServices)
    {
        InServices.Provide<IEditorService, EditorService>();
    }
    
    void EditorApplication::TickFrame()
    {
        IEditorService& lEditor = Editor();
        
        lEditor.BeginFrame();
        Engine().Loop();
        lEditor.EndFrame();
    }
    
    void EditorApplication::OnModulesRegistered()
    {
        Editor().RegisterExtensions([this](EditorExtensionRegistrar& InRegistrar)
        {
            OnRegisterEditorModules(InRegistrar);
        });
    }
    
    TUniquePtr<IPaths> EditorApplication::CreatePaths(const IPlatform& InPlatform, int InArgc, char** InArgv)
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
        const std::string lN = lName.CStr();
        const OpaaxString  lProjRel((lN + "/" + lN + ".opaaxproj").c_str());
        OPAAX_LOG(LogEditorApp, Info, "Editing project '{}' -> {}", lN, lProjRel.CStr());

        return MakeUnique<EditorPaths>(InPlatform, InArgc, InArgv, lProjRel);
    }
    
    WorldSpec EditorApplication::GetStartupWorldSpec() const
    {
        WorldSpec lSpec = OpaaxApplication::GetStartupWorldSpec();
       
        lSpec.Mode = EWorldMode::Edit;

        return lSpec;
    }

    void EditorApplication::PreRegisterConfig(IConfigSystem& ConfigSystem)
    {
        OpaaxApplication::PreRegisterConfig(ConfigSystem);

        ConfigSystem.Register<Config_EditorImgui>();
    }

    void EditorApplication::PostEngineStartup()
    {
        if (!Editor().IsNull())
        {
            Editor().Initialize();
        }
    }
    
    void EditorApplication::OnEvent(Event& InEvent)
    {
        if (Editor().RouteInput(InEvent))
        {
            return;
        }

        OpaaxApplication::OnEvent(InEvent);
    }
}
