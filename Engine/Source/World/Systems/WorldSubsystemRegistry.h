#pragma once

#include <functional>   // std::ref — see TWorldSubsystemEntry::CreateInto
#include <type_traits>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Application/Services/ILogger.h"

#include "World/Systems/WorldSubsystem.h"

namespace Opaax
{
    class World;
    struct WorldContext;

    inline constexpr LogCategory LogWorldSubsystemRegistry{"WorldSubsystemRegistry"};

    // =============================================================================
    // IWorldSubsystemEntry — the type-erased view of one registered world-subsystem type.
    //   Same shape as IComponentEntry, for the same reason: the engine stores and calls these
    //   through an exported vtable while a GAME MODULE instantiates the concrete entry.
    // =============================================================================
    class OPAAX_API IWorldSubsystemEntry
    {
    public:
        virtual ~IWorldSubsystemEntry() = default;

        /** Authoring name — logs and editor UI. Derived from the type when not given. */
        virtual OpaaxStringID GetName() const = 0;

        /**
         * Does this candidate belong in InWorld? Evaluated at EVERY world creation, including
         * every PIE start, so it must stay pure and cheap.
         *
         * STATIC by design: deciding needs no instance, so a world that rejects a candidate
         * never constructs one. That is what makes "an Edit-only overlay does not EXIST in a
         * Play world" true rather than merely inactive.
         */
        virtual bool ShouldCreate(const World& InWorld) const = 0;

        /**
         * Register this type into InManager, constructed from InContext.
         *
         * Goes through ISubsystemManager::RegisterSubsystem<T> (which forwards ctor args), so
         * the manager needs NO change to carry a context — the entry is what remembers T.
         */
        virtual void CreateInto(WorldSubsystemMgr& InManager, WorldContext& InContext) const = 0;
    };

    // =============================================================================
    // TWorldSubsystemEntry<T> — the concrete entry, baked from T at the registration site.
    //   NO OPAAX_API: exporting a class TEMPLATE exports nothing and makes every consumer
    //   expect an instantiation the DLL never emits (I6 — proven twice, most recently by
    //   ISubsystemManager in M4 S1). Header-only and per-TU is exactly what lets a game
    //   module in a static lib register its own subsystem types.
    // =============================================================================
    template<typename T>
    requires std::is_base_of_v<IWorldSubsystem, T>
    class TWorldSubsystemEntry final : public IWorldSubsystemEntry
    {
    public:
        explicit TWorldSubsystemEntry(OpaaxStringID InName) : m_Name(InName) {}

        OpaaxStringID GetName() const override { return m_Name; }

        bool ShouldCreate(const World& InWorld) const override
        {
            // OPTIONAL opt-in: a type that declares `static bool ShouldCreate(const World&)`
            // gets asked; everything else is created unconditionally. Most subsystems want
            // "always", and making them all write `return true` would be pure boilerplate —
            // while a selective one (an Edit-only overlay) still costs exactly one function.
            if constexpr (requires { { T::ShouldCreate(InWorld) } -> std::convertible_to<bool>; })
            {
                return T::ShouldCreate(InWorld);
            }
            else
            {
                return true;
            }
        }

        void CreateInto(WorldSubsystemMgr& InManager, WorldContext& InContext) const override
        {
            // T's ctor must accept a WorldContext& — the injection point (D3: no locator
            // downstream). A type that doesn't fails to compile HERE, at its own registration.
            //
            // std::ref is LOAD-BEARING, not style. RegisterSubsystem captures its ctor args BY
            // VALUE into the factory lambda, and StartupAll clears m_Factories once it has
            // consumed them — so passing InContext directly would copy the context into a lambda
            // that is then destroyed, leaving every subsystem that stored `WorldContext&`
            // pointing at freed memory. A reference_wrapper copies a POINTER to the World-owned
            // context (stable for the world's whole life) and converts back to WorldContext& for
            // the ctor. It also happens not to compile the other way, which is the only reason
            // this was caught rather than shipped.
            InManager.RegisterSubsystem<T>(std::ref(InContext));
        }

    private:
        OpaaxStringID m_Name;
    };

    // =============================================================================
    // WorldSubsystemRegistry — every world-subsystem type the engine and the loaded modules
    //   know about, in registration order. The second member of EngineRegistries (MR0), which
    //   is why it is engine-owned type metadata rather than WorldManager's private state.
    //
    //   These are CANDIDATES, not instances. One registry serves every world: each world walks
    //   the list at creation and takes the candidates whose ShouldCreate accepts it. That is the
    //   whole mechanism behind "a world runs a FILTERED set of subsystems".
    //
    //   SEALING: closes at the first CreateWorld, like ComponentRegistry — a type registered
    //   once a world exists would be silently missing from it.
    // =============================================================================
    class OPAAX_API WorldSubsystemRegistry
    {
        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        WorldSubsystemRegistry()  = default;
        ~WorldSubsystemRegistry() = default;

        // =========================================================================
        // Copy - Move Delete
        // =========================================================================
        //
        // REQUIRED by OPAAX_API, not hygiene: dllexport instantiates every implicitly-declared
        // member, and copy-assigning a TDynArray<TUniquePtr<...>> fails to compile (C2280) even
        // though nothing ever copies a registry. Same shape as ComponentRegistry and World.
        WorldSubsystemRegistry(const WorldSubsystemRegistry&)            = delete;
        WorldSubsystemRegistry& operator=(const WorldSubsystemRegistry&) = delete;
        WorldSubsystemRegistry(WorldSubsystemRegistry&&)                 = delete;
        WorldSubsystemRegistry& operator=(WorldSubsystemRegistry&&)      = delete;

        // =========================================================================
        // Registration
        // =========================================================================
    public:
        /**
         * Register T as a world-subsystem candidate.
         *
         * The name is REQUIRED here and derived by the caller when omitted — deriving it needs
         * `entt::type_name`, and the route already includes entt while this header has no other
         * reason to. Exactly how ComponentRoute feeds ComponentRegistry.
         *
         * @tparam T Derives IWorldSubsystem and is constructible from WorldContext&.
         * @param InName The authoring name. Refused (and logged) when invalid.
         * @return true when the registry accepted it.
         */
        template<typename T>
        requires std::is_base_of_v<IWorldSubsystem, T>
        bool Register(OpaaxStringID InName)
        {
            // NOTE: the entry is built here (T is known in the module's TU) but handed to an
            // out-of-line sink, so the entry LIST is only ever touched DLL-side. Same split as
            // ComponentRegistry::Register.
            return AddEntry(MakeUnique<TWorldSubsystemEntry<T>>(InName), InName);
        }

        /** Idempotent. Called by WorldManager::CreateWorld — after this, Register refuses. */
        void Seal() noexcept;

        // =========================================================================
        // Lookup
        // =========================================================================
    public:
        /** @return the entry registered under InName, or nullptr. */
        const IWorldSubsystemEntry* FindByName(OpaaxStringID InName) const noexcept;

        /** Iterate every candidate in registration order — what CreateWorld walks. */
        template<typename TFunc>
        void ForEach(TFunc&& InFunc) const
        {
            for (const TUniquePtr<IWorldSubsystemEntry>& lEntry : m_Entries)
            {
                InFunc(static_cast<const IWorldSubsystemEntry&>(*lEntry));
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
        bool AddEntry(TUniquePtr<IWorldSubsystemEntry> InEntry, OpaaxStringID InName);

        // =========================================================================
        // Members
        // =========================================================================
    private:
        TDynArray<TUniquePtr<IWorldSubsystemEntry>> m_Entries;
        bool                                       m_bSealed = false;
    };
}
