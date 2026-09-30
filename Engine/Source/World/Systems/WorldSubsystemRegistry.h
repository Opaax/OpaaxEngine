#pragma once

#include <functional>   // std::ref
#include <type_traits>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/Log/Logger.h"

#include "World/Systems/WorldSubsystem.h"

namespace Opaax
{
    class World;
    struct WorldContext;

    inline constexpr LogCategory LogWorldSubsystemRegistry{"WorldSubsystemRegistry"};

    // =============================================================================
    // IWorldSubsystemEntry — type-erased entry for one registered world subsystem type.
    // =============================================================================
    class OPAAX_API IWorldSubsystemEntry
    {
    public:
        virtual ~IWorldSubsystemEntry() = default;

        /** Name, for logs and the editor. Defaults to the type name. */
        virtual OpaaxStringID GetName() const = 0;

        /**
         * Whether this type is created in InWorld. Called at every world creation (keep it cheap).
         * A rejected type is never constructed.
         */
        virtual bool ShouldCreate(const World& InWorld) const = 0;

        /**
         * Registers this type into InManager, constructed from InContext.
         */
        virtual void CreateInto(WorldSubsystemMgr& InManager, WorldContext& InContext) const = 0;
    };

    // =============================================================================
    // TWorldSubsystemEntry<T> — concrete entry for T. Header-only (no OPAAX_API), so game
    //   modules can register their own types.
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
            // Optional: a type with static bool ShouldCreate(const World&) is asked; others are always created.
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
            // T must be constructible from WorldContext&.
            // std::ref: the factory captures its arguments by value, and a copy would leave subsystems
            // with dangling references.
            InManager.RegisterSubsystem<T>(std::ref(InContext));
        }

    private:
        OpaaxStringID m_Name;
    };

    // =============================================================================
    // WorldSubsystemRegistry — every world subsystem type, in registration order.
    //   Each new world creates the ones whose ShouldCreate accepts it.
    //   Sealed at the first CreateWorld.
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
        // Required by OPAAX_API: the implicit copy would not compile (C2280).
        WorldSubsystemRegistry(const WorldSubsystemRegistry&)            = delete;
        WorldSubsystemRegistry& operator=(const WorldSubsystemRegistry&) = delete;
        WorldSubsystemRegistry(WorldSubsystemRegistry&&)                 = delete;
        WorldSubsystemRegistry& operator=(WorldSubsystemRegistry&&)      = delete;

        // =========================================================================
        // Registration
        // =========================================================================
    public:
        /**
         * Registers T as a world subsystem type.
         * @tparam T Derives IWorldSubsystem, constructible from WorldContext&
         * @param InName Name (the route defaults it to the type name). Refused (and logged) if invalid.
         * @return True if registered
         */
        template<typename T>
        requires std::is_base_of_v<IWorldSubsystem, T>
        bool Register(OpaaxStringID InName)
        {
            // Built here (T is known) but stored by an out-of-line function in the DLL.
            return AddEntry(MakeUnique<TWorldSubsystemEntry<T>>(InName), InName);
        }

        /** Called by WorldManager::CreateWorld. After this, Register refuses. Safe to call twice. */
        void Seal() noexcept;

        // =========================================================================
        // Lookup
        // =========================================================================
    public:
        /** @return The entry registered under InName, or nullptr */
        const IWorldSubsystemEntry* FindByName(OpaaxStringID InName) const noexcept;

        /** Every entry, in registration order. */
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
        /** Stores an entry (takes ownership). */
        bool AddEntry(TUniquePtr<IWorldSubsystemEntry> InEntry, OpaaxStringID InName);

        // =========================================================================
        // Members
        // =========================================================================
    private:
        TDynArray<TUniquePtr<IWorldSubsystemEntry>> m_Entries;
        bool                                       m_bSealed = false;
    };
}
