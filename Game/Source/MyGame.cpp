#include "MyGame.h"
#include <iostream>

#include "Core/Input/InputSubsystem.h"
#include "Core/Log/OpaaxLog.h"
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

    // Solid colour quad
    Opaax::Renderer2D::DrawQuad({0.f, 0.f}, {100.f, 100.f}, {1.f, 0.2f, 0.2f, 1.f});
    Opaax::Renderer2D::DrawQuad({100.f, 0.f}, {100.f, 100.f}, {.7f, 0.35f, 0.2f, 1.f});
    Opaax::Renderer2D::DrawQuad({200.f, 0.f}, {100.f, 100.f}, {.12f, 0.25f, 0.9f, 1.f});

    //// Textured sprite (full texture)
    //Opaax::Renderer2D::DrawSprite({200.f, 0.f}, {64.f, 64.f}, *m_PlayerTexture);
    //
    //// Sprite sheet sub-region
    //Opaax::Renderer2D::DrawSprite({-200.f, 0.f}, {64.f, 64.f}, *m_Atlas,
    //                       {0.f, 0.f}, {0.25f, 0.25f});

    Opaax::Renderer2D::End();
}
