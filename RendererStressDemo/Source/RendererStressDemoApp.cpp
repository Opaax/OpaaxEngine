#include "RendererStressDemoApp.h"

#include "Core/Log/OpaaxLog.h"
#include "Core/OpaaxTypes.h"
#include "Renderer/Systems/WorldSubsystem.h"
#include "Scene/SceneFactory.h"
#include "Scene/SceneManager.h"

#include "Scene/StressScene.h"
#include "Systems/BulletStormRenderSystem.h"

using namespace Opaax;

RendererStressDemoApp::RendererStressDemoApp(int InArgc, char** InArgv) : Opaax::CoreEngineApp(InArgc, InArgv)
{
    OPAAX_TRACE("======================================");
    OPAAX_TRACE("Opaax Engine - RendererStressDemo Start");
    OPAAX_TRACE("======================================");
}

void RendererStressDemoApp::OnInitialize()
{
    OPAAX_TRACE("[RendererStressDemoApp] Initialize");

    // Required for PIE Stop to rebuild the scene stack.
    Opaax::SceneFactory::Register<StressScene>("StressScene");
}

void RendererStressDemoApp::OnStartup()
{
    OPAAX_TRACE("[RendererStressDemoApp] OnStartup");
    GetSubsystem<Opaax::SceneManager>()->Push(Opaax::MakeUnique<StressScene>());

    // Register the procedural stress drawer AFTER subsystems are up (WorldSubsystem exists only after
    // StartupAll) and create its GPU textures now — the render backend is live and we are outside a
    // frame (Lesson 33). Keep a typed handle across the ownership move to call EnsureTextures.
    auto                     lStress    = Opaax::MakeUnique<BulletStormRenderSystem>();
    BulletStormRenderSystem* lStressPtr = lStress.get();
    GetSubsystem<Opaax::WorldSubsystem>()->Register(Opaax::Move(lStress));
    lStressPtr->EnsureTextures();
}
