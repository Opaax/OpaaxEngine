#pragma once

#if OPAAX_WITH_EDITOR

#include "Editor/Inspector/IComponentDrawer.h"

namespace Opaax::Editor
{
    /**
     * @class MoverDrawer
     * Draws and edits the MoverComponent in the Inspector: capsule/circle proxy, slope limit,
     * the active mode id (read-only), and the MoverParams movement tuning.
     */
    class MoverDrawer final : public IComponentDrawer
    {
    public:
        bool HasComponent(World& InWorld, EntityID InEntity) const override;
        void Draw        (World& InWorld, EntityID InEntity)       override;
        void Add         (World& InWorld, EntityID InEntity)       override;

        const char* GetComponentName() const override { return "Mover"; }
        bool        CanAdd()           const override { return true; }
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
