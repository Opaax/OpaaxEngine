#include "PhysicsDemoApp.h"

#include "Core/Log/OpaaxLog.h"
#include "Core/OpaaxTypes.h"
#include "Scene/SceneFactory.h"
#include "Scene/SceneManager.h"

#include "Scene/PhysicsSandboxScene.h"
#include "Systems/SandboxControlSystem.h"
#include "Systems/PhysicsEventSystem.h"

PhysicsDemoApp::PhysicsDemoApp(int InArgc, char** InArgv) : Opaax::CoreEngineApp(InArgc, InArgv)
{
    OPAAX_TRACE("==================================");
    OPAAX_TRACE("Opaax Engine - PhysicsDemo Start");
    OPAAX_TRACE("==================================");
}

void PhysicsDemoApp::OnInitialize()
{
    OPAAX_TRACE("[PhysicsDemoApp] Initialize");

    // Required for PIE Stop to rebuild the scene stack.
    Opaax::SceneFactory::Register<PhysicsSandboxScene>("PhysicsSandboxScene");

    RegisterGameSubsystem<SandboxControlSystem>(this);
    RegisterGameSubsystem<PhysicsEventSystem>(this);
}

void PhysicsDemoApp::OnStartup()
{
    OPAAX_TRACE("[PhysicsDemoApp] OnStartup");
    GetSubsystem<Opaax::SceneManager>()->Push(Opaax::MakeUnique<PhysicsSandboxScene>());
}

void PhysicsDemoApp::OnUpdate(double /*DeltaTime*/)
{
}

void PhysicsDemoApp::OnShutdown()
{
    OPAAX_TRACE("[PhysicsDemoApp] OnShutdown");
}
