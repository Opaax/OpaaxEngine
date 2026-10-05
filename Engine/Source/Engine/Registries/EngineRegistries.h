#pragma once

#include "Core/EngineAPI.h"

#include "Engine/GameInstance/GameInstanceSubsystemRegistry.h"
#include "Engine/Subsystems/Resources/ResourceFormatRegistry.h"
#include "Engine/Subsystems/Resources/Types/DataAsset/DataAssetTypeRegistry.h"
#include "UI/UIWidgetRegistry.h"
#include "World/Components/ComponentRegistry.h"
#include "World/Systems/Movement/MoverModeRegistry.h"
#include "World/Systems/WorldSubsystemRegistry.h"

namespace Opaax
{
    // =============================================================================
    // EngineRegistries — every type registry the engine owns (components, subsystems,
    //   resource formats, ...). Owned by Engine. Only ModuleRegistrar writes to it,
    //   and only before it is sealed.
    // =============================================================================
    class OPAAX_API EngineRegistries
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
        // Required by OPAAX_API: the registries are not copyable.
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

        WorldSubsystemRegistry&       WorldSubsystems()       noexcept { return m_WorldSubsystems; }
        const WorldSubsystemRegistry& WorldSubsystems() const noexcept { return m_WorldSubsystems; }

        // Which resource type loads a file extension.
        ResourceFormatRegistry&       Resources()       noexcept { return m_ResourceFormats; }
        const ResourceFormatRegistry& Resources() const noexcept { return m_ResourceFormats; }

        // Movement modes a .opaaxmovemode can use.
        MoverModeRegistry&       MoverModes()       noexcept { return m_MoverModes; }
        const MoverModeRegistry& MoverModes() const noexcept { return m_MoverModes; }

        // Subsystems every game session creates.
        GameInstanceSubsystemRegistry&       GameInstanceSubsystems()       noexcept { return m_GameInstanceSubsystems; }
        const GameInstanceSubsystemRegistry& GameInstanceSubsystems() const noexcept { return m_GameInstanceSubsystems; }

        /** Widget types a .opaaxui can use. */
        UIWidgetRegistry&       UIWidgets()       noexcept { return m_UIWidgets; }
        const UIWidgetRegistry& UIWidgets() const noexcept { return m_UIWidgets; }

        /** Struct types a .opaaxdata can hold. */
        DataAssetTypeRegistry&       DataAssets()       noexcept { return m_DataAssets; }
        const DataAssetTypeRegistry& DataAssets() const noexcept { return m_DataAssets; }

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
            m_WorldSubsystems.Seal();
            m_ResourceFormats.Seal();
            m_MoverModes.Seal();
            m_GameInstanceSubsystems.Seal();
            m_UIWidgets.Seal();
            m_DataAssets.Seal();
        }

        // =========================================================================
        // Members
        // =========================================================================
    private:
        ComponentRegistry             m_Components;
        WorldSubsystemRegistry        m_WorldSubsystems;
        ResourceFormatRegistry        m_ResourceFormats;
        MoverModeRegistry             m_MoverModes;
        GameInstanceSubsystemRegistry m_GameInstanceSubsystems;
        UIWidgetRegistry              m_UIWidgets;
        DataAssetTypeRegistry         m_DataAssets;
    };
}
