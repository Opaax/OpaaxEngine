#pragma once

#include "Core/GUID/Guid.h"
#include "World/Entity/EntityTypes.h"

namespace Opaax
{
    class World;
}

namespace Opaax::Editor
{
    /** The ImGui payload type of an entity row being dragged (the Hierarchy, the Prefab panel's tree). */
    inline constexpr const char* ENTITY_DRAG_PAYLOAD = "OPAAX_ENTITY";

    /**
     * What a dragged entity row carries. The world says which document it comes from: a handle means
     * nothing in another world, so a drop from elsewhere is refused.
     */
    struct EntityDragPayload
    {
        EntityID     Handle = ENTITY_NONE;
        const World* Source = nullptr;
        Guid         Id;
    };
}
