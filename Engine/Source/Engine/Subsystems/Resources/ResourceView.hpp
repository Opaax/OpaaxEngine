#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

#include "ResourceConcept.hpp"
#include "ResourceHandle.hpp"
#include "ResourceManager.h"

// =============================================================================
// CheckedView<T> — a Resolve() result that asserts (debug) if used after the next Update(),
//   when the payload may have been destroyed. IsStale() works in every build.
//
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
         * @return True if an Update ran since this view was taken (do not dereference)
         */
        bool IsStale() const noexcept
        {
            return m_Mgr != nullptr && m_Mgr->GetPumpEpoch() != m_Epoch;
        }

        /***/
        T* Get() const noexcept
        {
            OPAAX_ASSERT(!IsStale()) // used after an Update
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
     * Resolves InHandle. Valid until the next Update().
     */
    template<CResource T>
    CheckedView<T> CheckedResolve(ResourceManager& InMgr, ResourceHandle<T> InHandle) noexcept
    {
        return CheckedView<T>{ &InMgr, InMgr.Resolve(InHandle), InMgr.GetPumpEpoch() };
    }
}
