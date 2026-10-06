#pragma once

#include "Core/OpaaxTypes.h"

#include "Resources/ResourceHandle.hpp"

// =============================================================================
// ResourceRef<T> — keeps a resource loaded (RAII, like shared_ptr). Used in code, not data.
//   Bodies that need the manager are defined in ResourceManager.h.
// =============================================================================
namespace Opaax
{
    class ResourceManager;

    template<typename T>
    class ResourceRef final
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        /** Empty ref. */
        ResourceRef() noexcept = default;
        
        /**
         * Takes the ref that Load/Pin already added. Refs come from Load()/Pin(), not built by hand.
         */
        ResourceRef(ResourceManager* InManager, ResourceHandle<T> InHandle) noexcept
            : m_Manager(InManager)
            , m_Handle(InHandle)
        {
        }

        // =============================================================================
        // Copy
        // =============================================================================
        
        ResourceRef(const ResourceRef& InOther);

        ResourceRef& operator=(const ResourceRef& InOther);

        // =============================================================================
        // Move
        // =============================================================================
        
        ResourceRef(ResourceRef&& InOther) noexcept
            : m_Manager(InOther.m_Manager)
            , m_Handle(InOther.m_Handle)
        {
            InOther.m_Manager = nullptr;
            InOther.m_Handle  = ResourceHandle<T>{};
        }

        ResourceRef& operator=(ResourceRef&& InOther) noexcept;

        ~ResourceRef();

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * @return The resource (manager->Resolve<T>)
         */
        T* Get() const noexcept;
        T* operator->() const noexcept { return Get(); }
        T& operator*()  const noexcept { return *Get(); }

        // =============================================================================
        // Get - Set
    public:
        ResourceHandle<T> GetHandle() const noexcept { return m_Handle; }
        bool              IsValid()   const noexcept { return m_Manager != nullptr && m_Handle.IsValid(); }
        explicit operator bool()      const noexcept { return IsValid(); }
        
        // End Get - Set
        // =============================================================================

        // =============================================================================
        // Members
        // =============================================================================
    private:
        ResourceManager*  m_Manager = nullptr;
        ResourceHandle<T> m_Handle{};
    };
}
