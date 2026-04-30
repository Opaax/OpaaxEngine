#pragma once

#if OPAAX_WITH_EDITOR

#include "Editor/Inspector/IComponentDrawer.h"

namespace Opaax::Editor
{
    /**
     * @class TagDrawer
     * Draws the TagComponent in the Inspector.
     * Not addable — every entity has a tag by default.
     */
    class TagDrawer final : public IComponentDrawer
    {
    public:
        bool HasComponent(World& InWorld, EntityID InEntity) const override;
        void Draw        (World& InWorld, EntityID InEntity)       override;

        const char* GetComponentName() const override { return "Tag"; }
        bool        CanAdd()           const override { return false; }
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
