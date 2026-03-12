#include "MyGame.h"
#include <iostream>

#include "Core/Log/OpaaxLog.h"

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
}

void MyGame::OnShutdown()
{
    OPAAX_TRACE("[MyGame] Shutdown - Game ended after {0} seconds", m_TotalTime);
}