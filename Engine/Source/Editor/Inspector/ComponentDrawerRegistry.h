#pragma once

#if OPAAX_WITH_EDITOR

#include "IComponentDrawer.h"
#include "Core/OpaaxTypes.h"

namespace Opaax::Editor
{
    /**
     * @class ComponentDrawerRegistry
     * Owns all registered component drawers.
     * Call Register() at startup (EditorSubsystem::Startup).
     * DrawAll() and DrawAddComponentMenu() are called by InspectorPanel each frame.
     */
    class OPAAX_API ComponentDrawerRegistry
    {
    public:
        static void Register(UniquePtr<IComponentDrawer> InDrawer);

        /**
         * Iterates all drawers; calls Draw() on each whose HasComponent() is true.
         */
        static void DrawAll(World& InWorld, EntityID InEntity);

        /**
         * Renders MenuItem entries for addable components not yet on the entity.
         * Must be called between ImGui::BeginPopup / EndPopup.
         */
        static void DrawAddComponentMenu(World& InWorld, EntityID InEntity);

        static void Clear();
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
