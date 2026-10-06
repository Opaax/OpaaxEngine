#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/Log/Logger.h"

#include "ResourceFormat.h"
#include "ResourceHold.hpp"    // ResourceHold, ResourceManager
#include "ResourceTypeID.hpp"

namespace Opaax
{
    inline constexpr LogCategory LogResourceFormatRegistry{"ResourceFormatRegistry"};

    // =============================================================================
    // ResourceFormatEntry — one registered resource type, as the table sees it.
    // =============================================================================
    struct ResourceFormatEntry
    {
        Uint32                TypeId = 0;        // ResourceTypeID::Get<T>()
        OpaaxStringID         Name;              // name, derived by the route
        const ResourceFormat* Format = nullptr;  // static constexpr, valid for the process

        /**
         * Loads a file of this type and returns a type-erased hold on it.
         */
        ResourceAcquireFn     Acquire = nullptr;
    };

    // =============================================================================
    // ResourceFormatRegistry — which resource type loads a file, by extension.
    //   One type can claim several extensions. Sealed before the first world.
    // =============================================================================
    class ResourceFormatRegistry
    {
        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        ResourceFormatRegistry()  = default;
        ~ResourceFormatRegistry() = default;

        // =========================================================================
        // Copy - Move Delete
        // =========================================================================
        //
        // Owns its entries: not copyable.
        ResourceFormatRegistry(const ResourceFormatRegistry&)            = delete;
        ResourceFormatRegistry& operator=(const ResourceFormatRegistry&) = delete;
        ResourceFormatRegistry(ResourceFormatRegistry&&)                 = delete;
        ResourceFormatRegistry& operator=(ResourceFormatRegistry&&)      = delete;

        // =========================================================================
        // Registration
        // =========================================================================
    public:
        /**
         * Registers T and every extension its OPAAX_RESOURCE_FORMAT lists. Refused (and logged)
         * if sealed, if InName is invalid, if T is already registered, or if an extension is taken.
         * @tparam T A resource type with OPAAX_RESOURCE_FORMAT
         * @param InName The name (the route defaults it to the type name)
         * @return True if registered
         */
        template<CResourceFormat T>
        bool Register(OpaaxStringID InName)
        {
            // Built here (T is known), stored by an out-of-line function.
            return AddEntry(ResourceTypeID::Get<T>(), InName, &T::Format, &AcquireHold<T>);
        }

        /** Called by EngineRegistries::SealAll. After this, Register refuses. Safe to call twice. */
        void Seal() noexcept;

        // =========================================================================
        // Lookup
        // =========================================================================
    public:
        /**
         * @param InExtension An id from NormalizeExtension (not raw text)
         * @return The type for InExtension, or nullptr
         */
        const ResourceFormatEntry* FindByExtension(OpaaxStringID InExtension) const noexcept;

        /** @return The entry for InTypeId (ResourceTypeID::Get<T>()), or nullptr */
        const ResourceFormatEntry* FindByTypeId(Uint32 InTypeId) const noexcept;

        /** Every entry, in registration order. */
        const TDynArray<ResourceFormatEntry>& Entries() const noexcept { return m_Entries; }

        // =========================================================================
        // Get - Set
        // =========================================================================
    public:
        bool IsSealed() const noexcept { return m_bSealed; }

        /** @return Number of registered types (not extensions) */
        Uint64 Count() const noexcept { return static_cast<Uint64>(m_Entries.size()); }

        // =========================================================================
        // Functions
        // =========================================================================
    private:
        /** Stores an entry. */
        bool AddEntry(Uint32 InTypeId, OpaaxStringID InName, const ResourceFormat* InFormat,
                      ResourceAcquireFn InAcquire);

        // =========================================================================
        // Members
        // =========================================================================
    private:
        TDynArray<ResourceFormatEntry> m_Entries;

        // Extension id -> index into m_Entries (not a pointer: the array reallocates).
        TUnorderedMap<Uint32, Uint64>  m_ByExtension;

        bool m_bSealed = false;
    };
}
