#pragma once

#include "Core/CoreEngineApp.h"

/**
 * @class PhysicsDemoApp
 *
 * Standalone consumer project (sibling to Game/ and SimpleShmup/) whose single scene
 * exercises every M9 physics feature end-to-end from a game author's seat: gravity +
 * resting dynamic bodies, static geometry, collision profiles, solid + overlap events,
 * ray/AABB queries, the world-bounds kill volume, the generic Mover (walk/jump/slide +
 * Ground/Fly mode switch), and live body removal when an entity is destroyed in a handler.
 */
class PhysicsDemoApp : public Opaax::CoreEngineApp
{
public:
    PhysicsDemoApp(int InArgc, char** InArgv);

protected:
    void OnInitialize() override;
    void OnStartup()    override;
    void OnUpdate(double DeltaTime) override;
    void OnShutdown()   override;
};
