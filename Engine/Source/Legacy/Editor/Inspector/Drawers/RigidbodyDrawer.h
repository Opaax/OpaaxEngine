#pragma once

#if OPAAX_WITH_EDITOR

#include "Editor/Inspector/IComponentDrawer.h"

namespace Opaax::Editor
{
    /**
     * @class RigidbodyDrawer
     * Draws and edits the RigidbodyComponent in the Inspector.
     */
    class RigidbodyDrawer final : public IComponentDrawer
    {
    public:
        bool HasComponent(WorldOld& InWorld, EntityID InEntity) const override;
        void Draw        (WorldOld& InWorld, EntityID InEntity)       override;
        void Add         (WorldOld& InWorld, EntityID InEntity)       override;

        const char* GetComponentName() const override { return "Rigidbody"; }
        bool        CanAdd()           const override { return true; }
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
