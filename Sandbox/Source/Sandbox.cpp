#include <Sandbox.h>

#include "Core/Application/Services/IConfigSystem.h"
#include "Core/Application/Services/ILogger.h"
#include "Core/Application/Services/IEngine.h"
#include "Core/Log/OpaaxLog.h"
#include "Core/World/WorldManager.h"
#include "Core/World/World.h"
#include "Core/World/Entity.h"
#include "Core/Components/DummyComponent.h"
#include "Config/ConfigTest.h"

Sandbox::Sandbox(int InArgc, char** InArgv) : Opaax::OpaaxApplication(InArgc, InArgv)
{
    //OPAAX_TRACE("-----------------------------------------------------");
    //OPAAX_TRACE("Opaax Application - Sandbox");
    //OPAAX_TRACE("-----------------------------------------------------");
}

void Sandbox::OnInitializeApplication()
{
    GetAppService<Opaax::ILogger>().Critical(Opaax::OpaaxString("Call from logger service"));
    GetAppService<Opaax::IConfigSystem>().Register<Config_MyConfig>();
}

void Sandbox::PostEngineStartup()
{
    // Populate the active world with a few dummy quads so the World -> Renderer path
    // is visible end-to-end (centered ortho: origin = screen centre, 1 unit = 1px).
    Opaax::World* lWorld = GetAppService<Opaax::IEngine>().GetWorldManager().GetActiveWorld();
    if (lWorld == nullptr)
    {
        return;
    }

    const auto lSpawn = [lWorld](const char* InName, Opaax::Vector2F InPos, Opaax::Vector4F InColor)
    {
        Opaax::Entity          lEntity = lWorld->CreateEntity(InName);
        Opaax::DummyComponent& lComp   = lEntity.Add<Opaax::DummyComponent>();
        lComp.Position = InPos;
        lComp.Size     = { 120.f, 120.f };
        lComp.Color    = InColor;
    };

    lSpawn("QuadRed",   { -200.f, 0.f }, { 1.f,  0.2f, 0.2f, 1.f });
    lSpawn("QuadGreen", {    0.f, 0.f }, { 0.2f, 1.f,  0.2f, 1.f });
    lSpawn("QuadBlue",  {  200.f, 0.f }, { 0.2f, 0.4f, 1.f,  1.f });
}
