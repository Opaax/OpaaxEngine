#pragma once

#include <nlohmann/json.hpp>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Application/Services/ILogger.h"

#include "Core/Reflection/OpaaxProperty.h"                  // ⑦-C P5 — CReflected + the property walk
#include "Engine/Subsystems/Resources/ResourcePath.h"       // ⑦-C P5 — k_IsHardResourcePath
#include "Engine/Subsystems/Resources/ResourceTypeID.hpp"   // ⑦-C P5b — a hard field names its type's id
#include "World/Components/ComponentConcept.hpp"
#include "World/Entity/EntityTypes.h"

#include <tuple>       // std::apply over the property list
#include <type_traits>

namespace Opaax
{
    inline constexpr LogCategory LogComponentRegistry{"ComponentRegistry"};

    /** One HARD resource field of a component: its json key, and which resource type it names. */
    struct HardRefField
    {
        OpaaxStringID Name;
        Uint32        TypeId = 0;   // ResourceTypeID::Get<T>() of the field's ResourceType
    };

    // =============================================================================
    // IComponentEntry — the type-erased view of one registered component type.
    //   Concept salvaged from Legacy/ECS/ComponentRegistry.h (Editor.md §6: "type-erased
    //   entry concept only"); none of its statics, WorldOld coupling, OOP virtuals or
    //   #if OPAAX_WITH_EDITOR drawer members came along.
    //
    //   Operates on the raw EntityRegistry rather than on World: the snapshot core needs
    //   to read and write components without a World method per type, and this keeps the
    //   entry free of any dependency on World's own API.
    // =============================================================================
    class OPAAX_API IComponentEntry
    {
    public:
        virtual ~IComponentEntry() = default;

        /** Stable authoring name — the key written to a map file (type ids are not portable). */
        virtual OpaaxStringID GetName() const = 0;

        /** entt's per-type id. Compiler-stable across the DLL boundary — see I2 and the probe suite. */
        virtual entt::id_type GetTypeId() const = 0;

        /** @return true if InEntity carries this component. */
        virtual bool Has(const EntityRegistry& InRegistry, EntityID InEntity) const = 0;

        /** Default-construct this component onto InEntity. No-op if already present. */
        virtual void Add(EntityRegistry& InRegistry, EntityID InEntity) const = 0;

        /**
         * Take this component off InEntity. No-op if absent, and REFUSED for an essential type.
         *
         * @return false when the type is essential — the caller may report it, but the guarantee is
         *   enforced here rather than by every UI remembering to check.
         */
        virtual bool Remove(EntityRegistry& InRegistry, EntityID InEntity) const = 0;

        /**
         * True when EVERY entity is guaranteed to carry this type, because World::CreateEntity
         * emplaces it. Such a component cannot be removed and is not worth offering in an "add"
         * menu: TransformComponent is the anchor picking, icons and the render joins all stand on,
         * so an entity without one would be invisible AND unclickable.
         */
        virtual bool IsEssential() const = 0;

        /** @return InEntity's component as json, or a null json when absent. */
        virtual nlohmann::json Save(const EntityRegistry& InRegistry, EntityID InEntity) const = 0;

        /** Deserialize onto InEntity, adding the component when it isn't there yet. */
        virtual void Load(EntityRegistry& InRegistry, EntityID InEntity, const nlohmann::json& InJson) const = 0;

        /**
         * This component's HARD resource references (P5): the json key of each, and the id of the
         * resource type it names — which is what lets a holder that reads untyped json turn the
         * field into a typed load (**PF11**, P5b) without anyone naming T again.
         *
         * A document loader reads its entities as untyped json, so it cannot tell a field that must
         * be resident up front from one that may wait. This is how it asks: the names are derived
         * from the type's own property list at registration, by TYPE, so declaring a field
         * `THardResourcePath<T>` is the entire opt-in and no list is maintained anywhere.
         *
         * EMPTY for most components, and cheaply so — a type with no properties, or none of them
         * hard, contributes nothing to walk.
         */
        virtual const TDynArray<HardRefField>& GetHardRefFields() const = 0;
    };

    // =============================================================================
    // TComponentEntry<T> — the concrete entry, baked from T at the registration site.
    //   NO OPAAX_API: this is a class TEMPLATE (I6 — exporting one exports nothing and
    //   makes every consumer expect an instantiation the DLL never emits). It is
    //   header-only and instantiated per-TU, which is exactly what lets a GAME MODULE
    //   register its own component types: the module instantiates the entry, the DLL
    //   stores and calls it through the exported IComponentEntry vtable.
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

            InRegistry.remove<T>(InEntity);   // entt's remove is a no-op when absent
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

    private:
        /**
         * Walk T's property list at construction and keep the names of its HARD resource fields.
         *
         * BY TYPE, with `if constexpr` — the property carries its `ValueType`, so a field opts in by
         * being declared `THardResourcePath<T>` and this needs no macro, no annotation and no list
         * (**I15**'s rule, which is also why a typed drop target costs no editor code).
         *
         * A type with no `OPAAX_PROPERTIES` is simply not reflected and contributes nothing — which
         * is correct rather than a gap: a component that describes no fields has no field anyone
         * could have marked.
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
                                        // The property NAME is the json key — the nlohmann macro
                                        // writes fields under their own names, so one string serves
                                        // the Inspector and the loader both. The type id needs T
                                        // NAMED, never complete (ResourcePath.h's own rule).
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

        /** P5 — see GetHardRefFields. Computed once, at registration. */
        TDynArray<HardRefField> m_HardRefFields;
    };

    // =============================================================================
    // ComponentRegistry — every component type the engine and the loaded game module know
    //   about, in registration order (engine natives first, module types after).
    //
    //   INSTANCE-OWNED (I1): owned by value by WorldManager, reached through
    //   GetSubsystem<WorldManager>(). The retired Legacy registry was entirely static;
    //   that is the part deliberately not salvaged.
    //
    //   One registry serves both consumers: the snapshot core (MapSerializer/MapFactory)
    //   and — later — the Inspector's "Add Component" menu.
    //
    //   SEALING (Editor.md §3 L1): the registry seals at the first CreateWorld. Registering
    //   after that is refused loudly, because a type registered once a world exists would
    //   silently be missing from every entity in it — the three-weeks-later bug.
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
        //
        // NOTE: these are not merely good hygiene here, they are REQUIRED by OPAAX_API.
        // dllexport instantiates every implicitly-declared member of the class, including the
        // copy-assignment operator — which then tries to copy a TDynArray<TUniquePtr<...>> and
        // fails to compile (C2280) even though nothing ever copies a registry. Declaring them
        // deleted stops the implicit generation. Same shape as World.
        ComponentRegistry(const ComponentRegistry&)            = delete;
        ComponentRegistry& operator=(const ComponentRegistry&) = delete;
        ComponentRegistry(ComponentRegistry&&)                 = delete;
        ComponentRegistry& operator=(ComponentRegistry&&)      = delete;

        // =========================================================================
        // Registration
        // =========================================================================
    public:
        /**
         * Register T under InName. Refused (and logged) when the registry is sealed, when
         * InName is invalid, or when either the name or the type is already taken.
         *
         * @tparam T Any type satisfying CComponent — no base class, no engine boilerplate.
         * @param InName The stable authoring name written to map files.
         * @param bInEssential True only for a type World::CreateEntity emplaces on EVERY entity —
         *   see IComponentEntry::IsEssential. Such a type cannot be removed. A game component is
         *   never this: the engine guarantees nothing about it.
         * @return true when the type was accepted.
         */
        template<CComponent T>
        bool Register(OpaaxStringID InName, bool bInEssential = false)
        {
            // NOTE: the entry is built here (so T is known) but handed to an out-of-line
            // sink, so the entry LIST is only ever touched DLL-side even when this template
            // is instantiated by a game module.
            return AddEntry(MakeUnique<TComponentEntry<T>>(InName, bInEssential),
                            entt::type_hash<T>::value());
        }

        /** Idempotent. Called by WorldManager::CreateWorld — after this, Register refuses. */
        void Seal() noexcept;

        // =========================================================================
        // Lookup
        // =========================================================================
    public:
        /** @return the entry registered under InName, or nullptr. */
        const IComponentEntry* FindByName(OpaaxStringID InName) const noexcept;

        /** @return the entry for InTypeId, or nullptr. */
        const IComponentEntry* FindByTypeId(entt::id_type InTypeId) const noexcept;

        /** Iterate every entry in registration order. */
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
        /** Out-of-line sink for Register<T> — see the NOTE there. Takes ownership. */
        bool AddEntry(TUniquePtr<IComponentEntry> InEntry, entt::id_type InTypeId);

        // =========================================================================
        // Members
        // =========================================================================
    private:
        TDynArray<TUniquePtr<IComponentEntry>> m_Entries;
        bool                                  m_bSealed = false;
    };
}
