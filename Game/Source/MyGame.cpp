#include "MyGame.h"
#include <iostream>

#include "Assets/AssetRegistry.h"
#include "Core/Input/InputSubsystem.h"
#include "Core/Log/OpaaxLog.h"
#include "Core/World/WorldRenderSystem.h"
#include "ECS/BaseComponents.hpp"
#include "Renderer/Renderer2D.h"
#include "RHI/RenderCommand.h"

MyGame::MyGame():Opaax::CoreEngineApp()
{
    OPAAX_TRACE("==================================");
    OPAAX_TRACE("Opaax Engine - Game Start");
    OPAAX_TRACE("==================================");
}

void MyGame::OnInitialize()
{
    OPAAX_TRACE("[MyGame] Initialize - Game starting!");
}

void MyGame::OnStartup()
{
    OPAAX_TRACE("[MyGame] OnStartup");

    m_PlayerTexture = Opaax::AssetRegistry::Load<Opaax::OpenGLTexture2D>(
        OPAAX_ASSET("EngineAssets/Textures/Player.png"));

    m_AtlasTexture = Opaax::AssetRegistry::Load<Opaax::OpenGLTexture2D>(
        OPAAX_ASSET("EngineAssets/Textures/PlayerSheet.png"));

    auto& lWorld = GetWorld();

    // Player entity
    auto lPlayer = lWorld.CreateEntity("Player");
    lWorld.AddComponent<Opaax::ECS::TransformComponent>(lPlayer,
        Opaax::ECS::TransformComponent{ .Position = {0.f, 0.f}, .Scale = {64.f, 64.f} });
    lWorld.AddComponent<Opaax::ECS::SpriteComponent>(lPlayer,
        Opaax::ECS::SpriteComponent{ .Texture = m_PlayerTexture });

    // Atlas entity — first tile of a 4x4 sheet
    auto lAtlas = lWorld.CreateEntity("AtlasSprite");
    lWorld.AddComponent<Opaax::ECS::TransformComponent>(lAtlas,
        Opaax::ECS::TransformComponent{ .Position = {100.f, 0.f}, .Scale = {64.f, 64.f} });
    lWorld.AddComponent<Opaax::ECS::SpriteComponent>(lAtlas,
        Opaax::ECS::SpriteComponent{
            .Texture = m_AtlasTexture,
            .UVMin   = {0.f,  0.f},
            .UVMax   = {0.25f, 0.25f}
        });

    // Coloured quads — no texture needed, solid colour via white pixel
    for (Opaax::Int32 i = 0; i < 3; ++i)
    {
        auto lQuad = lWorld.CreateEntity("ColorQuad");
        lWorld.AddComponent<Opaax::ECS::TransformComponent>(lQuad,
            Opaax::ECS::TransformComponent{
                .Position = { static_cast<float>(i) * 120.f, -150.f },
                .Scale    = { 100.f, 100.f }
            });
        // NOTE: No texture on these — WorldRenderSystem skips entities with no valid texture.
        //   DrawQuad path will be handled by a separate ColourComponent in a future pass.
    }
}

void MyGame::OnUpdate(double deltaTime)
{
    m_TotalTime += deltaTime;
    
    // Print every second
    static float printTimer = 0.0f;
    printTimer += deltaTime;
    
    if (printTimer >= 1.0f)
    {
        OPAAX_TRACE("[MyGame] Update - Running for {0} seconds", m_TotalTime);
        printTimer = 0.0f;
    }

    //OPAAX_TRACE("Input A Pressed? {0}", GetSubsystem<Opaax::InputSubsystem>()->IsKeyPressed(Opaax::EOpaaxKeyCode::A));
}

void MyGame::OnShutdown()
{
    OPAAX_TRACE("[MyGame] Shutdown - Game ended after {0} seconds", m_TotalTime);
}

void MyGame::OnRender(double AlphaPhysicStep)
{
    CoreEngineApp::OnRender(AlphaPhysicStep);

    Opaax::RenderCommand::SetClearColor(0.1f, 0.1f, 0.1f, 1.f);
    Opaax::RenderCommand::Clear();

    auto* lCamera = GetSubsystem<Opaax::Camera2D>();
    Opaax::Renderer2D::Begin(*lCamera);
    Opaax::ECS::WorldRenderSystem::Render(GetWorld());
    Opaax::Renderer2D::End();
}
