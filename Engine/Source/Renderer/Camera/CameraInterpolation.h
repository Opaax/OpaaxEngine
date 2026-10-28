#pragma once

#include "World/Entity/Entity.h"

namespace Opaax
{
    class World;

    // =============================================================================
    // CameraInterpolation — the OrthoSize a camera changed in fixed steps is framed with between
    //   them. CameraManager asks for it on every camera (RecordSteps): the world then records its
    //   size at the start and at the end of every fixed step (WorldManager::FixedUpdate), beside its
    //   pose (TransformInterpolation). Changed in Update instead, it is framed as it is.
    // =============================================================================
    namespace CameraInterpolation
    {
        /** Has InCamera's world record its OrthoSize around every fixed step. Calling it again does nothing. */
        void RecordSteps(Entity InCamera);

        /**
         * InCamera's next frame is a cut: its sizes and poses are forgotten (TransformInterpolation::
         * Teleport), so it is framed where it is, and the frame is marked. Call it after placing it.
         */
        void Cut(Entity InCamera);

        /** Start of a fixed step: the size of each camera recorded becomes its previous size. */
        void BeginFixedStep(World& InWorld);

        /** End of a fixed step: the size of each camera recorded is kept as the one the step left. */
        void EndFixedStep(World& InWorld);
    }
}
