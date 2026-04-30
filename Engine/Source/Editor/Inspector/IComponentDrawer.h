#pragma once

#if OPAAX_WITH_EDITOR

#include "Core/EngineAPI.h"
#include "Core/World/World.h"

namespace Opaax::Editor
{
    /**
     * @class IComponentDrawer
     * Per-component ImGui drawer interface.
     * Implement one concrete class per component type.
     * Register with ComponentDrawerRegistry::Register() at startup.
     *
     * Draw()  — called each frame when HasComponent() is true.
     * Add()   — called from the "Add Component" popup when CanAdd() is true and the component is absent.
     */
    class OPAAX_API IComponentDrawer
    {
        // =============================================================================
        // DTOR
        // =============================================================================
    public:
        virtual ~IComponentDrawer() = default;

        // =============================================================================
        // Function
        // =============================================================================

        virtual bool HasComponent(World& InWorld, EntityID InEntity) const = 0;
        virtual void Draw        (World& InWorld, EntityID InEntity)       = 0;

        virtual const char* GetComponentName()                       const = 0;
        virtual bool        CanAdd()                                 const { return false; }
        virtual void        Add(World& InWorld, EntityID InEntity)         {}
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
