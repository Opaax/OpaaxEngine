#pragma once

#include <concepts>
#include <optional>

#include "Core/OpaaxTypes.h"

// =============================================================================
// ResourceConcept — THE compile-time contract (replaces an IResource base class).
//
//   A resource is a PLAIN STRUCT that satisfies CResource: no vtable, no base
//   class, no two-phase init. The compiler rejects any type that doesn't provide
//   the required static interface. Game code defines resource types with ZERO
//   engine registration — satisfying the concept is the whole contract.
// =============================================================================
namespace Opaax
{
    // -------------------------------------------------------------------------
    // Failure policy — per type. Deciding question (review C7): does a degraded
    // substitute keep gameplay *correct*?
    //   Placeholder — renderables (pink texture), audio (silence), localization
    //                 (the key string): a degraded substitute is survivable.
    //   FailFast    — anything that DRIVES logic (Level, DialogueTree, gameplay
    //                 DBs): a placeholder there doesn't degrade, it lies. FailFast
    //                 propagates up hard-reference chains.
    // -------------------------------------------------------------------------
    enum class EFailPolicy : Uint8
    {
        Placeholder,
        FailFast
    };

    // -------------------------------------------------------------------------
    // Load state. Loading is reserved for the M-RES-2 async split (declared from
    // day one so the state machine never changes shape).
    // -------------------------------------------------------------------------
    enum class EResourceState : Uint8
    {
        Unloaded,
        Loading, // M-RES-2
        Loaded,
        Failed
    };

    // Composite loading is threaded through LoadContext& (defined elsewhere).
    class LoadContext;

    // =============================================================================
    // CResource — the contract every resource type must satisfy.
    // =============================================================================
    template<typename T>
    concept CResource = requires(const char* InPath, LoadContext& InCtx)
    {
        // Full object or nothing — no exceptions, no partially-built resources.
        { T::Load(InPath, InCtx) } -> std::same_as<std::optional<T>>;
        // Built once per pool (pink texture, silent clip, empty table...).
        { T::Placeholder() }       -> std::same_as<T>;
        // Placeholder vs FailFast.
        { T::FailPolicy }          -> std::convertible_to<EFailPolicy>;

        // NOTE M-RES-2: split Load -> LoadAnyThread + InitializeMainThread (GPU upload).
        // NOTE tools : static bool Save(const T&, const char* Path) joins at M-RES-ED.
        // Optional Uint64 ByteSize() const is detected at the pool with if-constexpr
        // (falls back to sizeof(T)) — not required by the contract.
    };
}
