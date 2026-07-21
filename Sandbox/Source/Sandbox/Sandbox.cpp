#include "Sandbox.h"

#include "Application/Modules/ModuleRegistrar.h"
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
    // NOTE: M0 demonstrative — DummyComponent is the one component the game currently uses (an engine
    // bring-up type). Registering it here proves the Components() route + the boot flow end to end.
    // Real Sandbox components replace it once they exist; the call site stays identical.
    InRegistrar.Components().Register<Opaax::DummyComponent>();

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
    };

    lSpawn("QuadRed",   { -200.f, 0.f }, { 1.f,  0.2f, 0.2f, 1.f });
    lSpawn("QuadGreen", {    0.f, 0.f }, { 0.2f, 1.f,  0.2f, 1.f });
    lSpawn("QuadBlue",  {  200.f, 0.f }, { 0.2f, 0.4f, 1.f,  1.f });
}
