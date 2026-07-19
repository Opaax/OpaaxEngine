#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

#include "ResourceConcept.hpp"
#include "ResourceHandle.hpp"
#include "ResourceManager.h"

// =============================================================================
// CheckedView<T> — a debug-guarded Resolve() result. It snapshots the manager's
// pump epoch when taken; a pump (Update) advances that epoch, after which a
// deferred unload may have destroyed the payload. Get()/operator-> assert in debug
// builds (via OPAAX_ASSERT) if used after a pump — catching the classic "cached a
// Resolve pointer across Update()" dangle. IsStale() is queryable in every build;
// the guard costs one Uint64 compare, so it is essentially free in release.
//
//   Opt-in helper OUTSIDE the frozen Load/Resolve/Pin/FlushAll/Update surface:
//   auto lView = CheckedResolve(mgr, handle); ... lView->Field;   // asserts if stale
// =============================================================================
namespace Opaax
{
    template<typename T>
    class CheckedView final
    {
        // =============================================================================
        // CTORS
        // =============================================================================
    public:
        CheckedView() = default;

        CheckedView(const ResourceManager* InMgr, T* InPtr, Uint64 InEpoch) noexcept
            : m_Mgr(InMgr)
            , m_Ptr(InPtr)
            , m_Epoch(InEpoch)
        {
        }

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * 
         * @return True once a pump has advanced past the epoch this view was taken in — the payload may have been collected, so the pointer must not be dereferenced.
         */
        bool IsStale() const noexcept
        {
            return m_Mgr != nullptr && m_Mgr->GetPumpEpoch() != m_Epoch;
        }

        /***/
        T* Get() const noexcept
        {
            OPAAX_ASSERT(!IsStale()) // debug: fired a Resolve pointer across a pump
            return m_Ptr;
        }
        /***/
        T* operator->() const noexcept { return Get(); }
        /***/
        T& operator*()  const noexcept { return *Get(); }
        /***/
        explicit operator bool() const noexcept { return m_Ptr != nullptr; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        const ResourceManager* m_Mgr   = nullptr;
        T*                     m_Ptr    = nullptr;
        Uint64                 m_Epoch  = 0;
    };
    
    /**
     * Resolve InHandle and wrap it with the current pump epoch. The result is valid to dereference only until the next Resources.Update().
     * @tparam T 
     * @param InMgr 
     * @param InHandle 
     * @return 
     */
    template<CResource T>
    CheckedView<T> CheckedResolve(ResourceManager& InMgr, ResourceHandle<T> InHandle) noexcept
    {
        return CheckedView<T>{ &InMgr, InMgr.Resolve(InHandle), InMgr.GetPumpEpoch() };
    }
}
