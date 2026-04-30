#pragma once

#if OPAAX_WITH_EDITOR

#include "Editor/Inspector/IComponentDrawer.h"

namespace Opaax::Editor
{
    /**
     * @class SpriteDrawer
     * Draws and edits the SpriteComponent in the Inspector.
     * Accepts ASSET_ID drag & drop to assign a Texture2D to the sprite.
     */
    class SpriteDrawer final : public IComponentDrawer
    {
    public:
        bool HasComponent(World& InWorld, EntityID InEntity) const override;
        void Draw        (World& InWorld, EntityID InEntity)       override;
        void Add         (World& InWorld, EntityID InEntity)       override;

        const char* GetComponentName() const override { return "Sprite"; }
        bool        CanAdd()           const override { return true; }
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
