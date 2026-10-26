

#include "Editor/Application/EditorApplication.h"

#include "Editor/Application/Services/EditorService.h"
#include "Editor/Application/Services/EditorPaths.h"
#include "Editor/Imgui/Configs/Config_EditorImgui.h"
#include "Editor/Panels/LogPanel.h"   // MAX_LINES

#include "Application/Services/IConfigSystem.h"

#include "Application/Services/IEngine.h"
#include "Application/Services/IPaths.h"
#include "Core/Log/Logger.h"
#include "Platform/CommandLine.h"
#include "Resources/Asset/AssetMigration.h"

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
        // Here, before Bootstrap, so the Log panel gets the boot lines. The game never enables it.
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
    
    TUniquePtr<IPaths> EditorApplication::CreatePaths(const IPlatform& InPlatform, const CommandLine& InCommandLine)
    {
        const OpaaxString lName = GetEditedProjectName();
        if (lName.IsEmpty())
        {
            // No project declared — fall back to the exe-stem default (usually wrong for an editor exe).
            OPAAX_LOG(LogEditorApp, Warn,
                "No edited project declared (override GetEditedProjectName); using the exe-stem default.");
            return OpaaxApplication::CreatePaths(InPlatform, InCommandLine);
        }

        // Layout convention: <name>/<name>.opaaxproj under the source workspace.
        const OpaaxString lProjRel = lName + "/" + lName + ".opaaxproj";
        OPAAX_LOG(LogEditorApp, Info, "Editing project '{}' -> {}", lName, lProjRel);

        return MakeUnique<EditorPaths>(InPlatform, InCommandLine, lProjRel);
    }

    void EditorApplication::OnPathsResolved(const IPaths& InPaths, const CommandLine& InCommandLine)
    {
        // The project's assets and the engine's (projects share it); configs name assets too.
        const TDynArray<AssetMigration::AssetRoot> lRoots = {
            { InPaths.AssetsDir(), OpaaxString() },
            { InPaths.EngineAssetsDir(), OpaaxString(ENGINE_MOUNT) }
        };

        if (!InCommandLine.HasFlag("--upgrade-assets"))
        {
            const Uint32 lOld = AssetMigration::CountLegacyFiles(lRoots);
            if (lOld > 0)
            {
                OPAAX_LOG(LogEditorApp, Warn,
                          "{} asset file(s) are in a format this engine no longer loads (.opaaxdata...): start the "
                          "editor once with --upgrade-assets to convert them to .opaaxasset", lOld);
            }
            return;
        }

        // Here, before the configs, the project and the levels are read: nothing in memory names an old file.
        const AssetMigration::Report lReport = AssetMigration::Migrate(lRoots, { InPaths.ConfigsDir() });

        OPAAX_LOG(LogEditorApp, Info, "Upgraded assets: {} converted, {} file(s) rewritten, {} error(s)",
                  static_cast<Uint64>(lReport.Converted.size()), static_cast<Uint64>(lReport.Rewritten.size()),
                  static_cast<Uint64>(lReport.Errors.size()));
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
    
    void EditorApplication::OnQuitGameRequested()
    {
        Editor().StopPlay();
    }

    void EditorApplication::OnAutomationStarted(AutomationRunner& InRunner)
    {
        Editor().RegisterAutomation(InRunner, IsInputScripted());
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
