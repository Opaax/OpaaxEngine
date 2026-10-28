#pragma once

#include "World/Entity/Entity.h"

namespace Opaax
{
    class World;

    // =============================================================================
    // TransformInterpolation — the poses an entity moved in fixed steps is drawn between.
    //   Physics and the mover record the previous pose of what they move
    //   (TransformInterpolationComponent). Any other entity that gameplay moves in a fixed step (a
    //   camera following a body, from a world subsystem or a behaviour's OnFixedUpdate) asks for it
    //   with RecordSteps: the world then records its pose at the start and at the end of every
    //   fixed step (WorldManager::FixedUpdate). Moved in Update instead, it is drawn where it was put.
    // =============================================================================
    namespace TransformInterpolation
    {
        /** Has InEntity's world record its pose around every fixed step. Calling it again does nothing. */
        void RecordSteps(Entity InEntity);

        /**
         * Stops recording InEntity's steps and forgets the poses recorded, so it is drawn where it is
         * until something records it again (physics, the mover, RecordSteps).
         */
        void StopRecordingSteps(Entity InEntity);

        /** Start of a fixed step: the pose of each entity recorded becomes its previous pose. */
        void BeginFixedStep(World& InWorld);

        /** End of a fixed step: the pose of each entity recorded is kept as the one the step left. */
        void EndFixedStep(World& InWorld);
    }
}
