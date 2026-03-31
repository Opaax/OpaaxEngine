#pragma once

#include "Core/CoreEngineApp.h"

class MyGame : public Opaax::CoreEngineApp
{
public:
    MyGame();
protected:
    void OnInitialize() override;
    void OnUpdate(double deltaTime) override;
    void OnShutdown() override;
    void OnRender(double AlphaPhysicStep) override;
    
private:
    float m_TotalTime = 0.0f;
};