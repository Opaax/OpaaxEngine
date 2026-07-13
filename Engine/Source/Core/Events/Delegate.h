#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Events/DelegateHandle.h"

namespace Opaax
{
    // =============================================================================
    // TDelegate — single-cast
    // =============================================================================

    /**
     * @class TDelegate
     * Single-cast, instance-bound callback holding at most one target (free function,
     * lambda, or a bound member). Tier-2 of the event system: point-to-point between
     * objects that already know each other. Not consumable, no "handled" concept.
     *
     * Header-only template — carries NO dllexport linkage (each module instantiates
     * its own copy; see the "never OPAAX_API a template" rule).
     */
    template<typename... Args>
    class TDelegate
    {
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** Bind a free function / lambda / functor. Replaces any existing target. */
        FORCEINLINE void Bind(TFunction<void(Args...)> InFunc)
        {
            m_Func = Move(InFunc);
        }

        /** Bind a non-const member function on InObj. Replaces any existing target. */
        template<typename T>
        FORCEINLINE void BindMember(T* InObj, void (T::*InMember)(Args...))
        {
            m_Func = [InObj, InMember](Args... InArgs) { (InObj->*InMember)(InArgs...); };
        }

        /** Drop the current target. */
        FORCEINLINE void Unbind() { m_Func = nullptr; }

        FORCEINLINE bool IsBound() const noexcept { return static_cast<bool>(m_Func); }

        // -----------------------------------------------------------------------------
        // Invocation
    public:
        /**
         * Invoke the bound target. Asserts (debug) when unbound — use ExecuteIfBound
         * when a missing target is a legal state.
         */
        FORCEINLINE void Execute(Args... InArgs) const
        {
            OPAAX_ASSERT(IsBound())
            m_Func(InArgs...);
        }

        /** Invoke the target only if one is bound. */
        FORCEINLINE void ExecuteIfBound(Args... InArgs) const
        {
            if (IsBound()) { m_Func(InArgs...); }
        }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TFunction<void(Args...)> m_Func;
    };

    // =============================================================================
    // TMulticastDelegate — one-to-many
    // =============================================================================

    /**
     * @class TMulticastDelegate
     * One-to-many callback list. Every bound listener is invoked on Broadcast, in
     * registration order; no "handled"/consumption concept (that is Tier 1). Each Add
     * returns a DelegateHandle the listener MUST keep and Remove before it dies — an
     * unremoved listener that is destroyed = dangling call on the next Broadcast.
     *
     * Broadcast is re-entrancy safe: it iterates a snapshot, so a handler may Add,
     * Remove, or RemoveAll during dispatch without invalidating iteration (a listener
     * removed mid-broadcast still runs for the in-flight broadcast).
     *
     * Header-only template — carries NO dllexport linkage.
     */
    template<typename... Args>
    class TMulticastDelegate
    {
        // =============================================================================
        // Types
        // =============================================================================
    private:
        struct HandlerEntry
        {
            DelegateHandle           Handle;
            void*                    Owner = nullptr;  // non-null for member binds; keyed by RemoveAll
            TFunction<void(Args...)> Func;
        };

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** Register a free function / lambda. @return handle for the matching Remove. */
        DelegateHandle Add(TFunction<void(Args...)> InFunc)
        {
            const DelegateHandle lHandle = DelegateHandle::Generate();
            m_Entries.push_back({lHandle, nullptr, Move(InFunc)});
            return lHandle;
        }

        /** Register a non-const member function on InObj. @return handle for Remove. */
        template<typename T>
        DelegateHandle AddMember(T* InObj, void (T::*InMember)(Args...))
        {
            const DelegateHandle lHandle = DelegateHandle::Generate();
            m_Entries.push_back({lHandle, InObj, [InObj, InMember](Args... InArgs) { (InObj->*InMember)(InArgs...); }});
            return lHandle;
        }

        /** Remove a single registration by handle. @return true if one was removed. */
        bool Remove(DelegateHandle InHandle)
        {
            for (auto lIt = m_Entries.begin(); lIt != m_Entries.end(); ++lIt)
            {
                if (lIt->Handle == InHandle)
                {
                    m_Entries.erase(lIt);
                    return true;
                }
            }
            return false;
        }

        /** Remove every registration owned by InObj (bulk-unbind by owner). */
        void RemoveAll(void* InObj)
        {
            for (auto lIt = m_Entries.begin(); lIt != m_Entries.end(); )
            {
                if (lIt->Owner == InObj) { lIt = m_Entries.erase(lIt); }
                else                     { ++lIt; }
            }
        }

        /** Drop all registrations. */
        void Clear() { m_Entries.clear(); }

        FORCEINLINE bool   IsBound() const noexcept { return !m_Entries.empty(); }
        FORCEINLINE Uint64 Num()     const noexcept { return static_cast<Uint64>(m_Entries.size()); }

        // -----------------------------------------------------------------------------
        // Invocation
    public:
        /** Invoke every bound listener in registration order (snapshot — re-entrant safe). */
        void Broadcast(Args... InArgs) const
        {
            const TDynArray<HandlerEntry> lSnapshot = m_Entries;
            for (const HandlerEntry& lEntry : lSnapshot)
            {
                lEntry.Func(InArgs...);
            }
        }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<HandlerEntry> m_Entries;
    };
}

// =============================================================================
// Declaration macros — Unreal-familiar surface.
//
// Each expands to a type alias of the underlying template; the alias name is
// caller-chosen. The trailing ';' is baked in, so the no-semicolon call form
// works: DECLARE_MULTICAST_DELEGATE_TwoParams(OnResized, Uint32, Uint32)
// Delegates are void-return only.
//
//   DECLARE_DELEGATE[_NParams]           -> TDelegate<...>          (single-cast)
//   DECLARE_MULTICAST_DELEGATE[_NParams] -> TMulticastDelegate<...> (one-to-many)
// =============================================================================

// -----------------------------------------------------------------------------
// Single-cast
#define DECLARE_DELEGATE(DelegateName) \
    using DelegateName = ::Opaax::TDelegate<>;

#define DECLARE_DELEGATE_OneParam(DelegateName, Arg1) \
    using DelegateName = ::Opaax::TDelegate<Arg1>;

#define DECLARE_DELEGATE_TwoParams(DelegateName, Arg1, Arg2) \
    using DelegateName = ::Opaax::TDelegate<Arg1, Arg2>;

#define DECLARE_DELEGATE_ThreeParams(DelegateName, Arg1, Arg2, Arg3) \
    using DelegateName = ::Opaax::TDelegate<Arg1, Arg2, Arg3>;

#define DECLARE_DELEGATE_FourParams(DelegateName, Arg1, Arg2, Arg3, Arg4) \
    using DelegateName = ::Opaax::TDelegate<Arg1, Arg2, Arg3, Arg4>;

// -----------------------------------------------------------------------------
// Multicast
#define DECLARE_MULTICAST_DELEGATE(DelegateName) \
    using DelegateName = ::Opaax::TMulticastDelegate<>;

#define DECLARE_MULTICAST_DELEGATE_OneParam(DelegateName, Arg1) \
    using DelegateName = ::Opaax::TMulticastDelegate<Arg1>;

#define DECLARE_MULTICAST_DELEGATE_TwoParams(DelegateName, Arg1, Arg2) \
    using DelegateName = ::Opaax::TMulticastDelegate<Arg1, Arg2>;

#define DECLARE_MULTICAST_DELEGATE_ThreeParams(DelegateName, Arg1, Arg2, Arg3) \
    using DelegateName = ::Opaax::TMulticastDelegate<Arg1, Arg2, Arg3>;

#define DECLARE_MULTICAST_DELEGATE_FourParams(DelegateName, Arg1, Arg2, Arg3, Arg4) \
    using DelegateName = ::Opaax::TMulticastDelegate<Arg1, Arg2, Arg3, Arg4>;
