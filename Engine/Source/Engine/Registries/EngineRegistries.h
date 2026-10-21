#pragma once

#include "Core/EngineAPI.h"

#include "Object/ObjectClassRegistry.h"
#include "Resources/ResourceFormatRegistry.h"
#include "UI/UIWidgetRegistry.h"
#include "World/Components/ComponentRegistry.h"

namespace Opaax
{
    // =============================================================================
    // EngineRegistries — every type registry the engine owns (object classes, components,
    //   resource formats, ...). Owned by Engine. Only ModuleRegistrar writes to it,
    //   and only before it is sealed.
    // =============================================================================
    class EngineRegistries
    {
        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        EngineRegistries()  = default;
        ~EngineRegistries() = default;

        // =========================================================================
        // Copy - Move Delete
        // =========================================================================
        // The registries are not copyable.
        EngineRegistries(const EngineRegistries&)            = delete;
        EngineRegistries& operator=(const EngineRegistries&) = delete;
        EngineRegistries(EngineRegistries&&)                 = delete;
        EngineRegistries& operator=(EngineRegistries&&)      = delete;

        // =========================================================================
        // Get - Set
        // =========================================================================
    public:
        ComponentRegistry&       Components()       noexcept { return m_Components; }
        const ComponentRegistry& Components() const noexcept { return m_Components; }

        // Which resource type loads a file extension.
        ResourceFormatRegistry&       Resources()       noexcept { return m_ResourceFormats; }
        const ResourceFormatRegistry& Resources() const noexcept { return m_ResourceFormats; }

        /** Widget types a .opaaxui can use. */
        UIWidgetRegistry&       UIWidgets()       noexcept { return m_UIWidgets; }
        const UIWidgetRegistry& UIWidgets() const noexcept { return m_UIWidgets; }

        /** Object classes: what files name, and the subsystems games and worlds make. */
        ObjectClassRegistry&       ObjectClasses()       noexcept { return m_ObjectClasses; }
        const ObjectClassRegistry& ObjectClasses() const noexcept { return m_ObjectClasses; }

        // =========================================================================
        // Functions
        // =========================================================================
    public:
        /**
         * Closes every registry. Called before the first world is created. Safe to call twice.
         */
        void SealAll() noexcept
        {
            m_Components.Seal();
            m_ResourceFormats.Seal();
            m_UIWidgets.Seal();
            m_ObjectClasses.Seal();
        }

        // =========================================================================
        // Members
        // =========================================================================
    private:
        ComponentRegistry             m_Components;
        ResourceFormatRegistry        m_ResourceFormats;
        UIWidgetRegistry              m_UIWidgets;
        ObjectClassRegistry           m_ObjectClasses;
    };
}
