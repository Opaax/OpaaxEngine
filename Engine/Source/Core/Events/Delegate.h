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
     * Single-cast callback: at most one target (function, lambda or member function).
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
         * Invokes the target. Asserts if unbound; use ExecuteIfBound when that is allowed.
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
     * Multi-cast callback list, invoked in registration order on Broadcast.
     * Keep the handle from Add and Remove it before the listener is destroyed.
     * Safe to Add/Remove during a Broadcast.
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
            void*                    Owner = nullptr;  // set for member binds (used by RemoveAll)
            TFunction<void(Args...)> Func;
        };

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** Adds a function or lambda. @return Handle for Remove. */
        DelegateHandle Add(TFunction<void(Args...)> InFunc)
        {
            const DelegateHandle lHandle = DelegateHandle::Generate();
            m_Entries.emplace_back(lHandle, nullptr, Move(InFunc));
            return lHandle;
        }

        /** Adds a member function on InObj. @return Handle for Remove. */
        template<typename T>
        DelegateHandle AddMember(T* InObj, void (T::*InMember)(Args...))
        {
            const DelegateHandle lHandle = DelegateHandle::Generate();
            m_Entries.emplace_back(lHandle, InObj, [InObj, InMember](Args... InArgs) { (InObj->*InMember)(InArgs...); });
            return lHandle;
        }

        /** Removes one listener. @return True if one was removed. */
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

        /** Removes every listener owned by InObj. */
        void RemoveAll(void* InObj)
        {
            for (auto lIt = m_Entries.begin(); lIt != m_Entries.end(); )
            {
                if (lIt->Owner == InObj) { lIt = m_Entries.erase(lIt); }
                else                     { ++lIt; }
            }
        }

        /** Removes all listeners. */
        void Clear() { m_Entries.clear(); }

        FORCEINLINE bool   IsBound() const noexcept { return !m_Entries.empty(); }
        FORCEINLINE Uint64 Num()     const noexcept { return static_cast<Uint64>(m_Entries.size()); }

        // -----------------------------------------------------------------------------
        // Invocation
    public:
        /** Invokes every listener in registration order. */
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
// Declaration macros (like Unreal). Each declares a type alias; void return only.
//   DECLARE_MULTICAST_DELEGATE_TwoParams(OnResized, Uint32, Uint32)
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
