#include "Sandbox.h"

#include "Engine/Registries/ModuleRegistrar.h"
#include "Components/GunComponent.h"
#include "Components/HealthComponent.h"
#include "Components/TagsComponent.h"
#include "Resources/WaveResource.h"
#include "Data/EnemyStats.h"
#include "Data/WeaponStats.h"
#include "Components/EnemyComponent.h"
#include "Systems/HudSubsystem.h"
#include "Systems/PauseMenuSubsystem.h"
#include "Systems/PlayerControlSubsystem.h"
#include "Systems/QuadOscillatorSubsystem.h"
#include "Core/Log/Logger.h"  // OPAAX_LOG + LogCategory

using namespace Opaax;

namespace
{
    constexpr LogCategory LogSandboxModule{"SandboxModule"};
}

void SandboxModule::OnRegister(Opaax::ModuleRegistrar& InRegistrar)
{
    // A game-owned component: saved and loaded like any engine type. (QuadComponent is an engine
    // type, registered by the engine.)
    InRegistrar.Components().Register<Sandbox::HealthComponent>();

    // Tags are saved as an array of strings, readable in the .opaaxmap.
    InRegistrar.Components().Register<Sandbox::TagsComponent>();

    // A hard reference: a level loading a map with a gun keeps its bullet prefab loaded.
    InRegistrar.Components().Register<Sandbox::GunComponent>();

    // A Play-only world subsystem (never constructed in an Edit world). Currently not registered.
    //InRegistrar.WorldSubsystems().Register<Sandbox::QuadOscillatorSubsystem>();

    // The game's side of the mover: input actions in, MoverInput out.
    InRegistrar.WorldSubsystems().Register<Sandbox::PlayerControlSubsystem>();

    // The HUD: a jump counter and a speed bar under the GameInstance's canvas.
    InRegistrar.WorldSubsystems().Register<Sandbox::HudSubsystem>();

    // The pause menu: a HUD button (GameAndUI) and a dimmed modal (UIOnly).
    InRegistrar.WorldSubsystems().Register<Sandbox::PauseMenuSubsystem>();

    // A resource type the engine does not know, with its own extension. Registered here (not
    // editor-side) because a wave is game content, so Sandbox.exe needs it too.
    InRegistrar.Resources().Register<Sandbox::WaveResource>();

    // A data asset type: this line is all it takes to get .opaaxdata files of it, editable in the editor.
    InRegistrar.DataAssets().Register<Sandbox::EnemyStats>();
    InRegistrar.DataAssets().Register<Sandbox::WeaponStats>();

    // A component pointing at one of those assets (the editor draws it with no code of its own).
    InRegistrar.Components().Register<Sandbox::EnemyComponent>();

    OPAAX_LOG(LogSandboxModule, Info,
        "RegisterModule: components={}, worldSubsystems={}, resourceFormats={}",
        InRegistrar.Components().Count(), InRegistrar.WorldSubsystems().Count(),
        InRegistrar.Resources().Count());
}

// The world's content is data: Sandbox/Assets/Maps/Main.opaaxmap, composed by
// Assets/Levels/Main.opaaxlevel (the project's startupLevel), opened by IEngine::FinishStartup
// for both hosts.
