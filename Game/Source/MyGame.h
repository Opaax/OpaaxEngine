#pragma once

#include "Assets/AssetHandle.hpp"
#include "Core/CoreEngineApp.h"

class MyGame : public Opaax::CoreEngineApp
{
public:
    MyGame();
    
protected:
    void OnInitialize() override;
    void OnStartup() override;
    void OnUpdate(double deltaTime) override;
    void OnShutdown() override;
    void OnRender(double AlphaPhysicStep) override;
};