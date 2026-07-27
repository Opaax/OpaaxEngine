#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/EngineAPI.h"
#include "Core/OpaaxGlobal.h"
#include "OpaaxString.hpp"

namespace Opaax
{
    /**
     * @class OpaaxStringIDPool
     *
     * The intern table backing every OpaaxStringID. Declared here, DEFINED in OpaaxStringID.cpp —
     * it is a DLL implementation detail on purpose (see OpaaxStringID's note on I2). Nothing outside
     * the engine DLL can see its layout, so nothing outside can accidentally operate on it.
     */
    class OpaaxStringIDPool;

    /**
     * @struct OpaaxStringID
     *
     * Interned string handle. Comparison is always O(1) integer compare.
     * Construction from a string is O(1) amortized (hash lookup + possible insert).
     *
     * Use OPAAX_ID("MyString") to construct at callsites.
     *
     * DLL SAFETY (I2) — every member that touches the intern pool is defined OUT-OF-LINE in the
     * engine DLL, never inline in this header. That is load-bearing, not style: an inline body here
     * would be compiled into each consuming module, and MSVC is free to emit a module-local copy of
     * the function-local `static` pool rather than import the DLL's. Each module would then get its
     * own intern table, and the SAME string would receive a DIFFERENT Uint32 on either side of the
     * DLL/exe line — so two ids that must compare equal would silently differ. Keeping the pool and
     * its entry points in the .cpp makes that impossible by construction rather than unlikely: a
     * consumer has no definition to duplicate. Only pool-free members (comparison, GetId, IsValid,
     * the Uint32 ctor) stay inline.
     */
    struct OPAAX_API OpaaxStringID final
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        OpaaxStringID() noexcept : m_ID(OpaaxGlobal::ID_None) {}

        /*** Interns InString — out-of-line, so the interning always runs in the DLL (see class note). */
        explicit OpaaxStringID(const OpaaxString& InString);

        explicit OpaaxStringID(Uint32 InID) noexcept : m_ID(InID) {}

        OpaaxStringID(const char*        InString) : OpaaxStringID(OpaaxString(InString)) {}
        OpaaxStringID(const std::string& InString) : OpaaxStringID(OpaaxString(InString.c_str())) {}

        OpaaxStringID(const OpaaxStringID&)            = default;
        OpaaxStringID(OpaaxStringID&&) noexcept        = default;
        OpaaxStringID& operator=(const OpaaxStringID&) = default;
        OpaaxStringID& operator=(OpaaxStringID&&) noexcept = default;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /*** The interned text, or "None" for an invalid id. Out-of-line — reads the pool. */
        OpaaxString ToString() const;

        /**
         * @return How many strings the process has interned (index 0 is the reserved "None").
         *   Diagnostic: it is what makes the one-pool-per-process invariant OBSERVABLE — interning a
         *   repeat must not grow it, and a second pool would show up here as a count that ignores
         *   what another module already interned.
         */
        static Uint32 PoolSize() noexcept;

        // ----------------------------------------------------------------------------
        // Get - Set
    public:
        FORCEINLINE Uint32 GetId()   const noexcept { return m_ID; }
        FORCEINLINE bool   IsValid() const noexcept { return m_ID != OpaaxGlobal::ID_None; }


        // =============================================================================
        // Operators
        // =============================================================================
    public:
        constexpr bool operator==(const OpaaxStringID& Other) const noexcept { return m_ID == Other.m_ID; }
        constexpr bool operator!=(const OpaaxStringID& Other) const noexcept { return m_ID != Other.m_ID; }

        // This does NOT intern the string — it is a read-only lookup.
        bool operator==(const OpaaxString& Other) const
        {
            return ToString() == Other;
        }

        bool operator!=(const OpaaxString& Other) const { return !(*this == Other); }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        Uint32 m_ID;

        // =============================================================================
        // Pool accessor — ONE instance per process. Declared only; defined in the .cpp so the
        // function-local static it owns cannot be duplicated per module (see the class note).
        // =============================================================================
        static OpaaxStringIDPool& GetPool();
    };

    // Convenience macro — keeps callsite noise down
    #define OPAAX_ID(STR) ::Opaax::OpaaxStringID(STR)

} // namespace Opaax

// fmtlib / spdlog formatter
#include <spdlog/fmt/fmt.h>

template<typename T>
struct fmt::formatter<T, std::enable_if_t<std::is_same_v<T, Opaax::OpaaxStringID>, char>>
    : fmt::formatter<std::string>
{
    auto format(const T& StringID, format_context& CTX) const
    {
        return fmt::formatter<std::string>::format(std::string(StringID.ToString().CStr()), CTX);
    }
};
