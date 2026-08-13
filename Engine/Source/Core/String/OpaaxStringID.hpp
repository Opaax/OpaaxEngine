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
     * consumer has no definition to duplicate. Only pool-free members (GetId, IsValid, id comparison,
     * the Uint32 ctor) stay inline.
     *
     * THREAD SAFETY — interning and reading are both safe from any thread, which the resource loader
     * relies on: it interns paths on a worker while the main thread logs the same names. Writes take
     * an exclusive lock, reads a shared one. Entries are immortal and address-stable, so the text a
     * reader is handed stays valid after the lock drops.
     *
     * LIFETIME — the pool is never destroyed (see the .cpp). A handle stays resolvable for the whole
     * process, including from a static destructor.
     *
     * CASE — comparison is case-SENSITIVE, unlike Unreal's FName. "Player" and "player" are two ids.
     * Call sites that want them unified fold case before interning, the way
     * Editor/Source/Editor/Resources/ResourceScan.cpp does for file extensions.
     */
    struct OPAAX_API OpaaxStringID final
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        constexpr OpaaxStringID() noexcept : m_ID(OpaaxGlobal::ID_None) {}

        /*** Interns InString — out-of-line, so the interning always runs in the DLL (see class note). */
        explicit OpaaxStringID(const OpaaxString& InString);

        constexpr explicit OpaaxStringID(Uint32 InID) noexcept : m_ID(InID) {}

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
         * The interned bytes, without copying them — the cheap form of ToString().CStr().
         * Valid for the life of the process: pool entries are immortal and never move.
         * This is not a second spelling of ToString (I11); it answers for the BYTES, the way
         * OpaaxString::CStr() does, and ToString() still returns a value as I11 requires.
         */
        const char* CStr() const;

        /**
         * The interned bytes AND their length, without copying or scanning for a terminator.
         * Valid for the life of the process, for the same reason CStr() is.
         *
         * This is what a caller doing text surgery on a name wants — OpaaxTag's hierarchy match
         * compares prefixes on every call, and CStr() alone would put a strlen in front of each one.
         */
        OpaaxStringView GetView() const;

        /**
         * The id of an ALREADY-interned string, or the invalid None id — it never adds an entry.
         * Unreal's FNAME_Find. Use it wherever the text is untrusted or unbounded (an editor text
         * field, a file scan): the table is never reclaimed, so interning a miss costs a permanent
         * slot for a string nobody will ask for twice.
         */
        static OpaaxStringID Find(const OpaaxString& InString);

        /**
         * @return How many strings the process has interned (index 0 is the reserved "None").
         *   Diagnostic: it is what makes the one-pool-per-process invariant OBSERVABLE — interning a
         *   repeat must not grow it, and a second pool would show up here as a count that ignores
         *   what another module already interned.
         */
        static Uint32 PoolSize();

        // ----------------------------------------------------------------------------
        // Get - Set
    public:
        FORCEINLINE constexpr Uint32 GetId()   const noexcept { return m_ID; }
        FORCEINLINE constexpr bool   IsValid() const noexcept { return m_ID != OpaaxGlobal::ID_None; }


        // =============================================================================
        // Operators
        // =============================================================================
    public:
        constexpr bool operator==(const OpaaxStringID& Other) const noexcept { return m_ID == Other.m_ID; }
        constexpr bool operator!=(const OpaaxStringID& Other) const noexcept { return m_ID != Other.m_ID; }

        // Read-only: this does NOT intern the string. Out-of-line — reads the pool, and compares
        // against the entry in place rather than allocating a copy of it first.
        bool operator==(const OpaaxString& Other) const;
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

// std::hash — so TUnorderedMap<OpaaxStringID, T> keys on the handle itself instead of on GetId().
// The id is already a well-distributed table index, so it IS the hash; nothing to mix.
template<>
struct std::hash<Opaax::OpaaxStringID>
{
    size_t operator()(const Opaax::OpaaxStringID& StringID) const noexcept
    {
        return static_cast<size_t>(StringID.GetId());
    }
};

// fmtlib / spdlog formatter — a VIEW over the pool's bytes, never a std::string copy.
// See the matching note in OpaaxString.hpp: copying here is what made `"{}", Id` more expensive than
// `"{}", Id.CStr()`, which is the wrong way round for the type whose whole point is cheap identity.
#include <spdlog/fmt/fmt.h>

template<typename T>
struct fmt::formatter<T, std::enable_if_t<std::is_same_v<T, Opaax::OpaaxStringID>, char>>
    : fmt::formatter<fmt::string_view>
{
    auto format(const T& StringID, format_context& CTX) const
    {
        return fmt::formatter<fmt::string_view>::format(fmt::string_view(StringID.CStr()), CTX);
    }
};
