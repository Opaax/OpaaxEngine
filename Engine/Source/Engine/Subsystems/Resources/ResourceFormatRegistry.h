#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Application/Services/ILogger.h"

#include "ResourceFormat.h"
#include "ResourceHold.hpp"    // the one thing an entry can CALL (P5b) — brings ResourceManager.h
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
        OpaaxStringID         Name;              // authoring / log name, derived by the route
        const ResourceFormat* Format = nullptr;  // -> a static constexpr; valid for the process

        /**
         * Load one file of this type and hand back an erased claim on it (P5b). THE ONE THING AN
         * ENTRY CAN CALL: a hard reference is read from a component's json as a path and a type
         * id, and this is how the id becomes a typed Load without anyone naming T again.
         */
        ResourceAcquireFn     Acquire = nullptr;
    };

    // =============================================================================
    // ResourceFormatRegistry — "which resource type loads this file?", answered by extension.
    //
    //   ONE erased call, unlike its two siblings' many. IComponentEntry / IWorldSubsystemEntry
    //   exist because the engine CALLS through them (Save, CreateInto); a format was pure data
    //   until ⑦-C P5b needed "load a file of type #N" — so an entry carries exactly that pointer
    //   and nothing else. The editor's own ResourceTypeRegistry still needs no erasure at all.
    //
    //   MANY extensions map to ONE type. Register<TextureResource>() claims .png, .jpg and .tga
    //   in one call, which is what keeps a new spelling out of every consumer downstream.
    //
    //   SEALING: seals with the other registries on the way to the first world (MR0/BO4). A type
    //   registered after that would be missing from a session that already scanned its files.
    // =============================================================================
    class OPAAX_API ResourceFormatRegistry
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
        // A registry is not a value, and OPAAX_API instantiates every implicitly-declared member
        // (I6 corollary) — the same shape ComponentRegistry uses.
        ResourceFormatRegistry(const ResourceFormatRegistry&)            = delete;
        ResourceFormatRegistry& operator=(const ResourceFormatRegistry&) = delete;
        ResourceFormatRegistry(ResourceFormatRegistry&&)                 = delete;
        ResourceFormatRegistry& operator=(ResourceFormatRegistry&&)      = delete;

        // =========================================================================
        // Registration
        // =========================================================================
    public:
        /**
         * Register T and claim every extension its `OPAAX_RESOURCE_FORMAT` names. Refused (and
         * logged) when the registry is sealed, when InName is invalid, when the type is already
         * registered, or when ANY of its extensions is already claimed by another type.
         *
         * Calls ResourceTypeID::Get<T>() eagerly — which is what gives the type its dense id
         * before anything loads it, since the pools otherwise mint ids on the first Load<T>.
         *
         * @tparam T A resource type carrying OPAAX_RESOURCE_FORMAT.
         * @param InName The authoring name; the route derives it from the type when omitted.
         * @return true when the type was accepted.
         */
        template<CResourceFormat T>
        bool Register(OpaaxStringID InName)
        {
            // NOTE: the call is instantiated wherever T is known (a game module, the exe), but the
            // entry LIST is only ever touched by the out-of-line sink below — i.e. DLL-side. Same
            // arrangement as ComponentRegistry::Register.
            return AddEntry(ResourceTypeID::Get<T>(), InName, &T::Format, &AcquireHold<T>);
        }

        /** Idempotent. Called by EngineRegistries::SealAll — after this, Register refuses. */
        void Seal() noexcept;

        // =========================================================================
        // Lookup
        // =========================================================================
    public:
        /**
         * @param InExtension An id from NormalizeExtension — NOT raw text. A plain integer lookup,
         *   deliberately: the file browser calls this per file per frame, and the scanner already
         *   normalized once at scan time. Raw text goes through NormalizeExtension first.
         * @return The type claiming InExtension, or nullptr.
         */
        const ResourceFormatEntry* FindByExtension(OpaaxStringID InExtension) const noexcept;

        /** @return The entry for InTypeId (ResourceTypeID::Get<T>()), or nullptr. */
        const ResourceFormatEntry* FindByTypeId(Uint32 InTypeId) const noexcept;

        /** Iterate every entry in registration order. */
        const TDynArray<ResourceFormatEntry>& Entries() const noexcept { return m_Entries; }

        // =========================================================================
        // Get - Set
        // =========================================================================
    public:
        bool IsSealed() const noexcept { return m_bSealed; }

        /** @return How many TYPES are registered — not how many extensions they claim between them. */
        Uint64 Count() const noexcept { return static_cast<Uint64>(m_Entries.size()); }

        // =========================================================================
        // Functions
        // =========================================================================
    private:
        /** Out-of-line sink for Register<T> — see the NOTE there. */
        bool AddEntry(Uint32 InTypeId, OpaaxStringID InName, const ResourceFormat* InFormat,
                      ResourceAcquireFn InAcquire);

        // =========================================================================
        // Members
        // =========================================================================
    private:
        TDynArray<ResourceFormatEntry> m_Entries;

        // Extension id -> INDEX into m_Entries, never a pointer: the array reallocates as types
        // register, and a stored pointer would name freed memory (the string-pool bug's shape, I2).
        TUnorderedMap<Uint32, Uint64>  m_ByExtension;

        bool m_bSealed = false;
    };
}
