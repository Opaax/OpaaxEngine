#pragma once

#if OPAAX_WITH_EDITOR

#include "Editor/Inspector/IComponentDrawer.h"

namespace Opaax::Editor
{
    /**
     * @class TransformDrawer
     * Draws and edits the TransformComponent in the Inspector.
     */
    class TransformDrawer final : public IComponentDrawer
    {
    public:
        bool HasComponent(World& InWorld, EntityID InEntity) const override;
        void Draw        (World& InWorld, EntityID InEntity)       override;
        void Add         (World& InWorld, EntityID InEntity)       override;

        const char* GetComponentName() const override { return "Transform"; }
        bool        CanAdd()           const override { return true; }

    private:
        void DrawParentPicker(World& InWorld, EntityID InEntity);
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
