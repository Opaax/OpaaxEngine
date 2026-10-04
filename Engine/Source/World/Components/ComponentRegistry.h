#pragma once

#include <nlohmann/json.hpp>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/Log/Logger.h"

#include "Core/Reflection/OpaaxProperty.h"
#include "Engine/Reflection/PropertyVisitor.h"
#include "Engine/Subsystems/Resources/ResourcePath.h"
#include "Engine/Subsystems/Resources/ResourceTypeID.hpp"
#include "World/Components/ComponentConcept.hpp"
#include "World/Entity/EntityTypes.h"

#include <tuple>       // std::apply
#include <type_traits>

namespace Opaax
{
    inline constexpr LogCategory LogComponentRegistry{"ComponentRegistry"};

    /** One hard resource field of a component: its JSON key and resource type id. */
    struct HardRefField
    {
        OpaaxStringID Name;
        Uint32        TypeId = 0;   // ResourceTypeID::Get<T>()
    };

    // =============================================================================
    // IComponentEntry — type-erased entry for one registered component type.
    //   Works on the raw EntityRegistry, so it needs no World method per type.
    // =============================================================================
    class OPAAX_API IComponentEntry
    {
    public:
        virtual ~IComponentEntry() = default;

        /** Name saved in map files. */
        virtual OpaaxStringID GetName() const = 0;

        /** entt's type id (stable across the DLL boundary). */
        virtual entt::id_type GetTypeId() const = 0;

        /** @return True if InEntity has this component */
        virtual bool Has(const EntityRegistry& InRegistry, EntityID InEntity) const = 0;

        /** Adds a default component to InEntity. Does nothing if already present. */
        virtual void Add(EntityRegistry& InRegistry, EntityID InEntity) const = 0;

        /**
         * Removes this component from InEntity. Does nothing if absent.
         * @return False for an essential type (cannot be removed)
         */
        virtual bool Remove(EntityRegistry& InRegistry, EntityID InEntity) const = 0;

        /**
         * True when every entity has this type (World::CreateEntity adds it, e.g. TransformComponent).
         * It cannot be removed and is not offered in the "Add Component" menu.
         */
        virtual bool IsEssential() const = 0;

        /** @return InEntity's component as JSON, or null JSON when absent */
        virtual nlohmann::json Save(const EntityRegistry& InRegistry, EntityID InEntity) const = 0;

        /** Reads the component onto InEntity, adding it if needed. */
        virtual void Load(EntityRegistry& InRegistry, EntityID InEntity, const nlohmann::json& InJson) const = 0;

        /**
         * This component's hard resource fields (THardResourcePath): JSON key and resource type id.
         * Lets a loader of untyped JSON load them up front. Found from the property list at registration.
         */
        virtual const TDynArray<HardRefField>& GetHardRefFields() const = 0;

        /** True when the type lists its fields (OPAAX_PROPERTIES), so it can be visited. */
        virtual bool IsReflected() const = 0;

        /**
         * Walks InEntity's component fields, in declaration order. Lets the editor draw a game's
         * component with no code written for it.
         * @return False when InEntity has no such component, or the type is not reflected
         */
        virtual bool VisitProperties(EntityRegistry& InRegistry, EntityID InEntity, IPropertyVisitor& InVisitor) const = 0;
    };

    // =============================================================================
    // TComponentEntry<T> — concrete entry for T. Header-only (no OPAAX_API), so game modules
    //   can register their own component types.
    // =============================================================================
    template<CComponent T>
    class TComponentEntry final : public IComponentEntry
    {
    public:
        TComponentEntry(OpaaxStringID InName, bool bInEssential)
            : m_Name(InName), m_bEssential(bInEssential)
        {
            CollectHardRefFields();
        }

        OpaaxStringID GetName()     const override { return m_Name; }
        entt::id_type GetTypeId()   const override { return entt::type_hash<T>::value(); }
        bool          IsEssential() const override { return m_bEssential; }

        bool Has(const EntityRegistry& InRegistry, EntityID InEntity) const override
        {
            return InRegistry.all_of<T>(InEntity);
        }

        void Add(EntityRegistry& InRegistry, EntityID InEntity) const override
        {
            if (!InRegistry.all_of<T>(InEntity))
            {
                InRegistry.emplace<T>(InEntity);
            }
        }

        bool Remove(EntityRegistry& InRegistry, EntityID InEntity) const override
        {
            if (m_bEssential)
            {
                return false;
            }

            InRegistry.remove<T>(InEntity);   // no-op when absent
            return true;
        }

        nlohmann::json Save(const EntityRegistry& InRegistry, EntityID InEntity) const override
        {
            const T* lComponent = InRegistry.try_get<T>(InEntity);
            return lComponent != nullptr ? nlohmann::json(*lComponent) : nlohmann::json{};
        }

        const TDynArray<HardRefField>& GetHardRefFields() const override { return m_HardRefFields; }

        void Load(EntityRegistry& InRegistry, EntityID InEntity, const nlohmann::json& InJson) const override
        {
            T& lComponent = InRegistry.get_or_emplace<T>(InEntity);
            InJson.get_to(lComponent);
        }

        bool IsReflected() const override { return CReflected<T>; }

        bool VisitProperties(EntityRegistry& InRegistry, EntityID InEntity, IPropertyVisitor& InVisitor) const override
        {
            if constexpr (CReflected<T>)
            {
                if (T* lComponent = InRegistry.try_get<T>(InEntity))
                {
                    ::Opaax::VisitProperties(*lComponent, InVisitor);
                    return true;
                }
            }
            return false;
        }

    private:
        /**
         * Collects the names of T's hard resource fields from its property list.
         * A type without OPAAX_PROPERTIES has none.
         */
        void CollectHardRefFields()
        {
            if constexpr (CReflected<T>)
            {
                std::apply([this](const auto&... lProperties)
                           {
                               ([this](const auto& InProperty)
                                {
                                    using TValue = typename std::decay_t<decltype(InProperty)>::ValueType;

                                    if constexpr (k_IsHardResourcePath<TValue>)
                                    {
                                        // The property name is also the JSON key.
                                        m_HardRefFields.emplace_back(
                                            OpaaxStringID(InProperty.Name),
                                            ResourceTypeID::Get<typename TValue::ResourceType>());
                                    }
                                }(lProperties), ...);
                           },
                           T::GetProperties());
            }
        }

        OpaaxStringID m_Name;
        bool          m_bEssential = false;

        /** See GetHardRefFields. Computed once, at registration. */
        TDynArray<HardRefField> m_HardRefFields;
    };

    // =============================================================================
    // ComponentRegistry — every component type (engine first, then game modules), in
    //   registration order. Owned by EngineRegistries. Used by map save/load and the
    //   Inspector's "Add Component" menu. Sealed at the first CreateWorld: later
    //   registrations are refused.
    // =============================================================================
    class OPAAX_API ComponentRegistry
    {
        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        ComponentRegistry()  = default;
        ~ComponentRegistry() = default;

        // =========================================================================
        // Copy - Move Delete
        // =========================================================================
        // Required by OPAAX_API: the implicit copy would not compile (C2280).
        ComponentRegistry(const ComponentRegistry&)            = delete;
        ComponentRegistry& operator=(const ComponentRegistry&) = delete;
        ComponentRegistry(ComponentRegistry&&)                 = delete;
        ComponentRegistry& operator=(ComponentRegistry&&)      = delete;

        // =========================================================================
        // Registration
        // =========================================================================
    public:
        /**
         * Registers T under InName. Refused (and logged) if sealed, if InName is invalid, or if the
         * name or type is already used.
         * @tparam T Any type satisfying CComponent
         * @param InName Name saved in map files
         * @param bInEssential True only for a type every entity has (added by World::CreateEntity)
         * @return True if registered
         */
        template<CComponent T>
        bool Register(OpaaxStringID InName, bool bInEssential = false)
        {
            // Built here (T is known) but stored by an out-of-line function in the DLL.
            return AddEntry(MakeUnique<TComponentEntry<T>>(InName, bInEssential),
                            entt::type_hash<T>::value());
        }

        /** Called by WorldManager::CreateWorld. After this, Register refuses. Safe to call twice. */
        void Seal() noexcept;

        // =========================================================================
        // Lookup
        // =========================================================================
    public:
        /** @return The entry registered under InName, or nullptr */
        const IComponentEntry* FindByName(OpaaxStringID InName) const noexcept;

        /** @return The entry for InTypeId, or nullptr */
        const IComponentEntry* FindByTypeId(entt::id_type InTypeId) const noexcept;

        /** Every entry, in registration order. */
        template<typename TFunc>
        void ForEach(TFunc&& InFunc) const
        {
            for (const TUniquePtr<IComponentEntry>& lEntry : m_Entries)
            {
                InFunc(static_cast<const IComponentEntry&>(*lEntry));
            }
        }

        // =========================================================================
        // Get - Set
        // =========================================================================
    public:
        bool   IsSealed() const noexcept { return m_bSealed; }
        Uint64 Count()    const noexcept { return static_cast<Uint64>(m_Entries.size()); }

        // =========================================================================
        // Functions
        // =========================================================================
    private:
        /** Stores an entry (takes ownership). */
        bool AddEntry(TUniquePtr<IComponentEntry> InEntry, entt::id_type InTypeId);

        // =========================================================================
        // Members
        // =========================================================================
    private:
        TDynArray<TUniquePtr<IComponentEntry>> m_Entries;
        bool                                  m_bSealed = false;
    };
}
