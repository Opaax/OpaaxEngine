#pragma once

#include "Assets/AssetHandle.hpp"
#include "Core/CoreEngineApp.h"

class MyGame : public Opaax::CoreEngineApp
{
public:
    MyGame(int InArgc, char** InArgv);

protected:
    void OnInitialize() override;
    void OnStartup() override;
    void OnUpdate(double deltaTime) override;
    void OnShutdown() override;
};