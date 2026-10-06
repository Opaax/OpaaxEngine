#pragma once

#include "Core/Log/Logger.h"
#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "World/Systems/Movement/IMoverMode.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(MoverModeRegistry);

    // =============================================================================
    // MoverModeRegistry — the movement modes a .opaaxmovemode can use.
    //   Modes are stateless: one instance of each, shared. Sealed before the first world.
    // =============================================================================
    class MoverModeRegistry
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
         * Registers T under InName (the name .opaaxmovemode files use).
         * @return False if sealed, if the name is invalid, or if it is already used (no replacing)
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

            return true;
        }

        /** Closes registration. Safe to call twice. */
        void Seal() noexcept { m_bSealed = true; }

        // =========================================================================
        // Get
        // =========================================================================
    public:
        /** The mode named InName, or nullptr. */
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
