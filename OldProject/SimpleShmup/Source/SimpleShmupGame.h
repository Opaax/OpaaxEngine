#pragma once

#include "Core/CoreEngineApp.h"

/**
 * @class SimpleShmupGame
 * Entry app for the G1 shmup vertical slice. Hosts Menu/Game scenes and the
 * persistent score entity once Step 7 lands.
 */
class SimpleShmupGame : public Opaax::CoreEngineApp
{
    // =============================================================================
    // CTORS - DTORS
    // =============================================================================
public:
    SimpleShmupGame(int InArgc, char** InArgv);

    // =============================================================================
    // Functions
    // =============================================================================
protected:
    void OnInitialize() override;
    void OnStartup()    override;
    void OnUpdate(double DeltaTime) override;
    void OnShutdown()   override;
};
