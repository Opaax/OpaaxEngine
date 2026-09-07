#pragma once

#include "Application/Services/ILogger.h"
#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "World/Systems/Movement/IMoverMode.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(MoverModeRegistry);

    // =============================================================================
    // MoverModeRegistry — which behaviours a `.opaaxmovemode` may name.
    //
    //   The fourth engine registry, and it sits in EngineRegistries beside the other three for
    //   that aggregate's stated reason: a registry is TYPE METADATA, not one subsystem's state.
    //   MoverSubsystem reads it; nothing owns it but the Engine.
    //
    //   MODES ARE STATELESS, so this owns ONE instance of each and every entity running that mode
    //   shares it. A tuning is what differs per entity, and that is a resource.
    //
    //   SEALED with its siblings on the way to the first world (**MR2**): a mode registered later
    //   would be missing from movers that already resolved against it.
    // =============================================================================
    class OPAAX_API MoverModeRegistry
    {
        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        MoverModeRegistry()  = default;
        ~MoverModeRegistry() = default;

        // =========================================================================
        // Copy - Move Delete
        // =========================================================================
        MoverModeRegistry(const MoverModeRegistry&)            = delete;
        MoverModeRegistry& operator=(const MoverModeRegistry&) = delete;
        MoverModeRegistry(MoverModeRegistry&&)                 = delete;
        MoverModeRegistry& operator=(MoverModeRegistry&&)      = delete;

        // =========================================================================
        // Registration
        // =========================================================================
    public:
        /**
         * Register T under InName — the id a `.opaaxmovemode` writes in its Mode field.
         *
         * @return false when sealed, when the name is invalid, or when it is already taken. A
         *   duplicate is REFUSED rather than replacing, so a game module cannot silently shadow a
         *   built-in mode that other assets already name.
         */
        template<typename T>
        requires std::is_base_of_v<IMoverMode, T>
        bool Register(const OpaaxStringID InName)
        {
            if (m_bSealed)
            {
                OPAAX_LOG(LogMoverModeRegistry, Error,
                          "Register '{}' refused — the registry is sealed", InName.CStr());
                return false;
            }

            if (!InName.IsValid())
            {
                OPAAX_LOG(LogMoverModeRegistry, Error, "Register refused — a mode needs a name");
                return false;
            }

            if (Find(InName) != nullptr)
            {
                OPAAX_LOG(LogMoverModeRegistry, Error,
                          "Register '{}' refused — that name is already a mode", InName.CStr());
                return false;
            }

            m_Entries.emplace_back(InName, MakeUnique<T>());

            OPAAX_LOG(LogMoverModeRegistry, Trace, "Registered mover mode '{}' ({} total)",
                      InName.CStr(), m_Entries.size());
            return true;
        }

        /** Close to further registration. Idempotent. */
        void Seal() noexcept { m_bSealed = true; }

        // =========================================================================
        // Get
        // =========================================================================
    public:
        /** The mode called InName, or nullptr. A name nothing registered is a real state. */
        IMoverMode* Find(OpaaxStringID InName) const noexcept;

        Uint64 Count()    const noexcept { return static_cast<Uint64>(m_Entries.size()); }
        bool   IsSealed() const noexcept { return m_bSealed; }

        // =========================================================================
        // Members
        // =========================================================================
    private:
        struct Entry
        {
            OpaaxStringID       Name;
            TUniquePtr<IMoverMode> Mode;
        };

        TDynArray<Entry> m_Entries;
        bool             m_bSealed = false;
    };
}
