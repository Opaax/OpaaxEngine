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
    // Load state. A slot is Loading between an async request and its main-thread
    // publish; Resolve gates on Loaded, so a Loading handle resolves to the fail
    // policy (placeholder/null) until the payload lands — the streaming behavior.
    // -------------------------------------------------------------------------
    enum class EResourceState : Uint8
    {
        Unloaded,
        Loading, // async request placed, payload not yet published
        Loaded,
        Failed
    };

    // Composite loading is threaded through LoadContext& (defined elsewhere).
    class LoadContext;

    // =============================================================================
    // CResource — the contract every resource type must satisfy.
    //
    //   Two-phase loading. Load is the ANY-THREAD producer: pure file IO + CPU
    //   decode (+ composite ctx.Acquire<Child>, which runs inline on the same
    //   worker). It must be self-contained — no GPU, no shared mutable state.
    //   The optional Initialize() below is the MAIN-THREAD "log-in" (GPU upload,
    //   handle registration) run once during the pump, after Load's payload lands.
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

        // Optional, detected at the pool with if-constexpr (not required here):
        //   void   Initialize()      — main-thread GPU log-in, run once in the pump.
        //   Uint64 ByteSize() const  — real payload bytes (else sizeof(T)).
    };
}
