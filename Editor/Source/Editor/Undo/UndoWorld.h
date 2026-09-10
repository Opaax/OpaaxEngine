#pragma once

#include "Core/OpaaxTypes.h"

namespace Opaax
{
    class World;
}

namespace Opaax::Editor
{
    struct EditorContext;

    /**
     * WHICH document's world a step acts on (⑦-C P8 V3). A step resolves its world at REPLAY time
     * (**UN2** — never a stored pointer, since the level's world is replaced by a PIE cycle), and
     * until the prefab panel there was only one answer. One field on the step, one resolver here,
     * instead of a second step type per document (**MP7**).
     */
    enum class EUndoWorld : Uint8
    {
        Active,   // the level — WorldManager's active world
        Prefab    // the prefab document's own world (**PF9**)
    };

    /** The world InScope names right now, or null. */
    World* UndoWorld(const EditorContext& InContext, EUndoWorld InScope);
}
