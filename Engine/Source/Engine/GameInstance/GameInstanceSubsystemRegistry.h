#pragma once

#include <functional>   // std::ref
#include <type_traits>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/Log/Logger.h"

#include "Engine/GameInstance/IGameInstanceSubsystem.h"

namespace Opaax
{
    struct GameInstanceContext;

    inline constexpr LogCategory LogGameInstanceSubsystemRegistry{"GameInstanceSubsystemRegistry"};

    // =============================================================================
    // IGameInstanceSubsystemEntry — type-erased entry for one registered game-instance subsystem type.
    // =============================================================================
    class OPAAX_API IGameInstanceSubsystemEntry
    {
    public:
        virtual ~IGameInstanceSubsystemEntry() = default;

        /** Name, for logs and the editor. */
        virtual OpaaxStringID GetName() const = 0;

        /**
         * Registers this type into InManager, constructed from InContext.
         */
        virtual void CreateInto(GameInstanceSubsystemMgr& InManager, GameInstanceContext& InContext) const = 0;
    };

    // =============================================================================
    // TGameInstanceSubsystemEntry<T> — concrete entry for T. Header-only (no OPAAX_API), so
    //   game modules can register their own types.
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
            // T must be constructible from GameInstanceContext&.
            // std::ref: the factory captures its arguments by value, and a copy would leave
            // subsystems with dangling references.
            InManager.RegisterSubsystem<T>(std::ref(InContext));
        }

    private:
        OpaaxStringID m_Name;
    };

    // =============================================================================
    // GameInstanceSubsystemRegistry — every game-instance subsystem type, in registration order.
    //   Every game creates all of them, in order. Closed (sealed) at the first CreateWorld.
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
        // Required by OPAAX_API: the implicit copy would not compile (C2280).
        GameInstanceSubsystemRegistry(const GameInstanceSubsystemRegistry&)            = delete;
        GameInstanceSubsystemRegistry& operator=(const GameInstanceSubsystemRegistry&) = delete;
        GameInstanceSubsystemRegistry(GameInstanceSubsystemRegistry&&)                 = delete;
        GameInstanceSubsystemRegistry& operator=(GameInstanceSubsystemRegistry&&)      = delete;

        // =========================================================================
        // Registration
        // =========================================================================
    public:
        /**
         * Registers T as a game-instance subsystem.
         * @tparam T Derives IGameInstanceSubsystem, constructible from GameInstanceContext&
         * @param InName Name. Refused (and logged) if invalid or already used.
         * @return True if registered
         */
        template<typename T>
        requires std::is_base_of_v<IGameInstanceSubsystem, T>
        bool Register(OpaaxStringID InName)
        {
            // Built here (T is known) but stored by an out-of-line function in the DLL.
            return AddEntry(MakeUnique<TGameInstanceSubsystemEntry<T>>(InName), InName);
        }

        /** Called by WorldManager::CreateWorld. After this, Register refuses. Safe to call twice. */
        void Seal() noexcept;

        // =========================================================================
        // Lookup
        // =========================================================================
    public:
        /** @return The entry registered under InName, or nullptr */
        const IGameInstanceSubsystemEntry* FindByName(OpaaxStringID InName) const noexcept;

        /** Every entry, in registration order. */
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
        /** Stores an entry (takes ownership). */
        bool AddEntry(TUniquePtr<IGameInstanceSubsystemEntry> InEntry, OpaaxStringID InName);

        // =========================================================================
        // Members
        // =========================================================================
    private:
        TDynArray<TUniquePtr<IGameInstanceSubsystemEntry>> m_Entries;
        bool                                              m_bSealed = false;
    };
}
