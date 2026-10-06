#pragma once

#if OPAAX_WITH_EDITOR

#include "Editor/Inspector/IComponentDrawer.h"

namespace Opaax::Editor
{
    /**
     * @class ColliderDrawer
     * Draws and edits the ColliderComponent in the Inspector.
     * Accepts ASSET_ID drag & drop to assign a CollisionProfile.
     */
    class ColliderDrawer final : public IComponentDrawer
    {
    public:
        bool HasComponent(WorldOld& InWorld, EntityID InEntity) const override;
        void Draw        (WorldOld& InWorld, EntityID InEntity)       override;
        void Add         (WorldOld& InWorld, EntityID InEntity)       override;

        const char* GetComponentName() const override { return "Collider"; }
        bool        CanAdd()           const override { return true; }
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
