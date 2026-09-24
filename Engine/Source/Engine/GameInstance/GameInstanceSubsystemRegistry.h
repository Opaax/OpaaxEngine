#pragma once

#include <functional>   // std::ref — see TGameInstanceSubsystemEntry::CreateInto
#include <type_traits>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Application/Services/ILogger.h"

#include "Engine/GameInstance/IGameInstanceSubsystem.h"

namespace Opaax
{
    struct GameInstanceContext;

    inline constexpr LogCategory LogGameInstanceSubsystemRegistry{"GameInstanceSubsystemRegistry"};

    // =============================================================================
    // IGameInstanceSubsystemEntry — the type-erased view of one registered game-instance
    //   subsystem type. IWorldSubsystemEntry's shape, for the same reason: the engine stores
    //   and calls these through an exported vtable while a GAME MODULE instantiates the
    //   concrete entry.
    //
    //   NO ShouldCreate, deliberately. A world FILTERS its candidates because Edit and Play
    //   worlds coexist and want different sets (WS2); there is only ever one kind of game, so
    //   a filter here would be a hook with nothing to decide.
    // =============================================================================
    class OPAAX_API IGameInstanceSubsystemEntry
    {
    public:
        virtual ~IGameInstanceSubsystemEntry() = default;

        /** Authoring name — logs and editor UI. */
        virtual OpaaxStringID GetName() const = 0;

        /**
         * Register this type into InManager, constructed from InContext.
         *
         * Goes through ISubsystemManager::RegisterSubsystem<T> (which forwards ctor args), so
         * the manager needs NO change to carry a context — the entry is what remembers T.
         */
        virtual void CreateInto(GameInstanceSubsystemMgr& InManager, GameInstanceContext& InContext) const = 0;
    };

    // =============================================================================
    // TGameInstanceSubsystemEntry<T> — the concrete entry, baked from T at the registration
    //   site. NO OPAAX_API: exporting a class TEMPLATE exports nothing and makes every
    //   consumer expect an instantiation the DLL never emits (I6). Header-only and per-TU is
    //   exactly what lets a game module in a static lib register its own subsystem types.
    // =============================================================================
    template<typename T>
    requires std::is_base_of_v<IGameInstanceSubsystem, T>
    class TGameInstanceSubsystemEntry final : public IGameInstanceSubsystemEntry
    {
    public:
        explicit TGameInstanceSubsystemEntry(OpaaxStringID InName) : m_Name(InName) {}

        OpaaxStringID GetName() const override { return m_Name; }

        void CreateInto(GameInstanceSubsystemMgr& InManager, GameInstanceContext& InContext) const override
        {
            // T's ctor must accept a GameInstanceContext& — the injection point (D3: no locator
            // downstream). A type that doesn't fails to compile HERE, at its own registration.
            //
            // std::ref is LOAD-BEARING, not style — WS4, one tier up. RegisterSubsystem captures
            // its ctor args BY VALUE into the factory lambda, and StartupAll clears m_Factories
            // once it has consumed them, so passing InContext directly would copy the context into
            // a lambda that is then destroyed and leave every stored reference dangling.
            InManager.RegisterSubsystem<T>(std::ref(InContext));
        }

    private:
        OpaaxStringID m_Name;
    };

    // =============================================================================
    // GameInstanceSubsystemRegistry — every game-instance-subsystem type the engine and the
    //   loaded modules know about, in registration order. The FIFTH member of
    //   EngineRegistries (MR0), engine-owned type metadata rather than any subsystem's state.
    //
    //   These are CANDIDATES, not instances. Every game creates the whole list, in order.
    //
    //   SEALING: closes at the first CreateWorld like its siblings. A game is started before
    //   the first world (BO4's fourth stage), so the window a module registers in is the same
    //   one components and world subsystems get, and it closes at the same moment.
    // =============================================================================
    class OPAAX_API GameInstanceSubsystemRegistry
    {
        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        GameInstanceSubsystemRegistry()  = default;
        ~GameInstanceSubsystemRegistry() = default;

        // =========================================================================
        // Copy - Move Delete
        // =========================================================================
        //
        // REQUIRED by OPAAX_API, not hygiene: dllexport instantiates every implicitly-declared
        // member, and copy-assigning a TDynArray<TUniquePtr<...>> fails to compile (C2280).
        GameInstanceSubsystemRegistry(const GameInstanceSubsystemRegistry&)            = delete;
        GameInstanceSubsystemRegistry& operator=(const GameInstanceSubsystemRegistry&) = delete;
        GameInstanceSubsystemRegistry(GameInstanceSubsystemRegistry&&)                 = delete;
        GameInstanceSubsystemRegistry& operator=(GameInstanceSubsystemRegistry&&)      = delete;

        // =========================================================================
        // Registration
        // =========================================================================
    public:
        /**
         * Register T as a game-instance-subsystem candidate.
         *
         * @tparam T Derives IGameInstanceSubsystem and is constructible from GameInstanceContext&.
         * @param InName The authoring name. Refused (and logged) when invalid or already taken.
         * @return true when the registry accepted it.
         */
        template<typename T>
        requires std::is_base_of_v<IGameInstanceSubsystem, T>
        bool Register(OpaaxStringID InName)
        {
            // NOTE: the entry is built here (T is known in the module's TU) but handed to an
            // out-of-line sink, so the entry LIST is only ever touched DLL-side.
            return AddEntry(MakeUnique<TGameInstanceSubsystemEntry<T>>(InName), InName);
        }

        /** Idempotent. Called by WorldManager::CreateWorld — after this, Register refuses. */
        void Seal() noexcept;

        // =========================================================================
        // Lookup
        // =========================================================================
    public:
        /** @return the entry registered under InName, or nullptr. */
        const IGameInstanceSubsystemEntry* FindByName(OpaaxStringID InName) const noexcept;

        /** Iterate every candidate in registration order — what StartGame walks. */
        template<typename TFunc>
        void ForEach(TFunc&& InFunc) const
        {
            for (const TUniquePtr<IGameInstanceSubsystemEntry>& lEntry : m_Entries)
            {
                InFunc(static_cast<const IGameInstanceSubsystemEntry&>(*lEntry));
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
        bool AddEntry(TUniquePtr<IGameInstanceSubsystemEntry> InEntry, OpaaxStringID InName);

        // =========================================================================
        // Members
        // =========================================================================
    private:
        TDynArray<TUniquePtr<IGameInstanceSubsystemEntry>> m_Entries;
        bool                                              m_bSealed = false;
    };
}
