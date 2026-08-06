#pragma once

#include <nlohmann/json.hpp>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Application/Services/ILogger.h"

#include "World/Components/ComponentConcept.hpp"
#include "World/Entity/EntityTypes.h"

namespace Opaax
{
    inline constexpr LogCategory LogComponentRegistry{"ComponentRegistry"};

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

        /** @return InEntity's component as json, or a null json when absent. */
        virtual nlohmann::json Save(const EntityRegistry& InRegistry, EntityID InEntity) const = 0;

        /** Deserialize onto InEntity, adding the component when it isn't there yet. */
        virtual void Load(EntityRegistry& InRegistry, EntityID InEntity, const nlohmann::json& InJson) const = 0;
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
        explicit TComponentEntry(OpaaxStringID InName) : m_Name(InName) {}

        OpaaxStringID GetName()   const override { return m_Name; }
        entt::id_type GetTypeId() const override { return entt::type_hash<T>::value(); }

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

        nlohmann::json Save(const EntityRegistry& InRegistry, EntityID InEntity) const override
        {
            const T* lComponent = InRegistry.try_get<T>(InEntity);
            return lComponent != nullptr ? nlohmann::json(*lComponent) : nlohmann::json{};
        }

        void Load(EntityRegistry& InRegistry, EntityID InEntity, const nlohmann::json& InJson) const override
        {
            T& lComponent = InRegistry.get_or_emplace<T>(InEntity);
            InJson.get_to(lComponent);
        }

    private:
        OpaaxStringID m_Name;
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
         * @return true when the type was accepted.
         */
        template<CComponent T>
        bool Register(OpaaxStringID InName)
        {
            // NOTE: the entry is built here (so T is known) but handed to an out-of-line
            // sink, so the entry LIST is only ever touched DLL-side even when this template
            // is instantiated by a game module.
            return AddEntry(MakeUnique<TComponentEntry<T>>(InName), entt::type_hash<T>::value());
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
