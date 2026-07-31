#include "Sandbox.h"

#include "Engine/Modules/ModuleRegistrar.h"
#include "Components/HealthComponent.h"
#include "Systems/QuadOscillatorSubsystem.h"
#include "World/Components/DummyComponent.h"
#include "Application/Services/ILogger.h"  // OPAAX_LOG + LogCategory
#include "World/World.h"
#include "World/Entity/Entity.h"

// OPAAX_LOG expands to an unqualified ToSpdLevel(...) — bring Opaax into scope, as engine TUs do.
using namespace Opaax;

namespace
{
    constexpr LogCategory LogSandboxModule{"SandboxModule"};
}

void SandboxModule::OnRegister(Opaax::ModuleRegistrar& InRegistrar)
{
    // A component the GAME owns — the engine has never heard of HealthComponent, yet it
    // round-trips through capture/instantiate like any native type. That is the whole point
    // of the route: satisfying CComponent is the entire contract.
    //
    // NOTE: DummyComponent is deliberately NOT registered here any more. It is an engine
    // type, and the engine registers its own natives first (MR2) — asking again would be
    // refused as a duplicate.
    InRegistrar.Components().Register<Sandbox::HealthComponent>();

    // A world subsystem the ENGINE has never heard of. Play-only: it exists in a Play world and is
    // never constructed in an Edit one (WS1/WS2). Registering costs one line — no base-class
    // ceremony, no reflection, no static-init.
    //
    // NOTE: the Edit-only QuadBoundsSubsystem is NOT here any more (M4 S5). An authoring overlay is
    // EDITOR content, so it moved to the SandboxEditor module and registers through
    // EditWorldSystems() — a different route into the SAME registry. Sandbox.exe therefore has one
    // candidate and SandboxEditor.exe has two, which is the model's point stated as a build fact.
    InRegistrar.WorldSubsystems().Register<Sandbox::QuadOscillatorSubsystem>();

    OPAAX_LOG(LogSandboxModule, Info,
        "RegisterModule: components={}, worldSubsystems={}",
        InRegistrar.Components().Count(), InRegistrar.WorldSubsystems().Count());
}

void SandboxModule::SpawnDemoWorld(Opaax::World& InWorld)
{
    // Centered ortho: origin = screen centre, 1 unit = 1px. Shared by both hosts (D9).
    const auto lSpawn = [&InWorld](const char* InName, Vector2F InPos, Vector4F InColor)
    {
        Entity          lEntity = InWorld.CreateEntity(InName);
        DummyComponent& lComp   = lEntity.Add<DummyComponent>();
        lComp.Position = InPos;
        lComp.Size     = { 120.f, 120.f };
        lComp.Color    = InColor;

        // Carried by the demo quads so the registered game component has a real user, not
        // just a registration.
        lEntity.Add<Sandbox::HealthComponent>();
    };

    lSpawn("QuadBlue",  {  200.f, 0.f }, { 0.2f, 0.4f, 1.f,  1.f });
    lSpawn("QuadWhite", {    0.f, 0.f }, { 1.f, 1.f,  1.f, 1.f });
    lSpawn("QuadRed",   { -200.f, 0.f }, { 1.f,  0.2f, 0.2f, 1.f });
}
