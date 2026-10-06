#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/EngineAPI.h"
#include "Core/OpaaxGlobal.h"
#include "OpaaxString.hpp"

namespace Opaax
{
    /**
     * The intern table behind OpaaxStringID. Defined in OpaaxStringID.cpp.
     */
    class OpaaxStringIDPool;

    /**
     * Interned string handle: O(1) comparison. Create with OPAAX_ID("MyString").
     * Case-sensitive. Thread-safe. Valid for the whole process.
     *
     * Members that use the pool are defined in the .cpp, so the whole program
     * shares the same table.
     */
    struct OpaaxStringID final
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        constexpr OpaaxStringID() noexcept : m_ID(OpaaxGlobal::ID_None) {}

        /** Interns InString. */
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
        /** The interned text, or "None" for an invalid id. */
        OpaaxString ToString() const;

        /**
         * The interned bytes, without copying. Valid for the whole process.
         */
        const char* CStr() const;

        /**
         * The interned bytes and their length, without copying. Valid for the whole process.
         */
        OpaaxStringView GetView() const;

        /**
         * Id of an already-interned string, or the invalid id. Never adds an entry:
         * use it for untrusted text (text fields, file scans).
         */
        static OpaaxStringID Find(const OpaaxString& InString);

        /**
         * @return Number of interned strings (index 0 is "None"). For diagnostics.
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

        // Does not intern the string.
        bool operator==(const OpaaxString& Other) const;
        bool operator!=(const OpaaxString& Other) const { return !(*this == Other); }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        Uint32 m_ID;

        // =============================================================================
        // The one pool. Defined in the .cpp so every module shares it.
        // =============================================================================
        static OpaaxStringIDPool& GetPool();
    };

    // Creates an OpaaxStringID
    #define OPAAX_ID(STR) ::Opaax::OpaaxStringID(STR)

} // namespace Opaax

// std::hash<OpaaxStringID>: the id is already a good hash.
template<>
struct std::hash<Opaax::OpaaxStringID>
{
    size_t operator()(const Opaax::OpaaxStringID& StringID) const noexcept
    {
        return static_cast<size_t>(StringID.GetId());
    }
};

// Formatter for spdlog / fmtlib (no copy).
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
