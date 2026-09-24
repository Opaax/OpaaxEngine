#pragma once

#include "Core/OpaaxTypes.h"

#include "ResourceHandle.hpp"

// =============================================================================
// ResourceRef<T> — ownership CLAIM (RAII), ~16B. Lives in CODE (game/engine
// systems), never in data. Mirrors shared_ptr: add-ref on copy, release on
// destruction. A Ref is a lifetime *claim*, not ownership — pools own everything.
//
//   Team rule: "in code it's a Ref, in data it's a Handle." Manual release calls
//   never appear in game code. Bodies that touch the manager (copy / assign / dtor
//   / Get) are declared here and DEFINED in ResourceManager.h, where the manager
//   is a complete type — this breaks the Ref<->Manager template cycle.
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
        /**  empty claim (null manager) */
        ResourceRef() noexcept = default;
        
        /**
         * Adopt the +1 that Load/Pin already applied — no extra add-ref.
         * Manager-only in spirit; game code receives Refs from Load()/Pin(), never builds them.
         * @param InManager 
         * @param InHandle 
         */
        ResourceRef(ResourceManager* InManager, ResourceHandle<T> InHandle) noexcept
            : m_Manager(InManager)
            , m_Handle(InHandle)
        {
        }

        // =============================================================================
        // Copy
        // =============================================================================
        
        /**
         * add-ref  — defined in ResourceManager.h
         * @param InOther 
         */
        ResourceRef(const ResourceRef& InOther);

        /**
         * release old + add-ref — defined in ResourceManager.h
         * @param InOther 
         * @return 
         */
        ResourceRef& operator=(const ResourceRef& InOther);

        // =============================================================================
        // Move
        // =============================================================================
        
        /***/
        ResourceRef(ResourceRef&& InOther) noexcept
            : m_Manager(InOther.m_Manager)
            , m_Handle(InOther.m_Handle)
        {
            InOther.m_Manager = nullptr;
            InOther.m_Handle  = ResourceHandle<T>{};
        }

        /** release old + steal — defined in ResourceManager.h */
        ResourceRef& operator=(ResourceRef&& InOther) noexcept;

        /** release — defined in ResourceManager.h */
        ~ResourceRef();

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * manager->Resolve<T> — defined in ResourceManager.h
         * @return 
         */
        T* Get() const noexcept;
        T* operator->() const noexcept { return Get(); }
        T& operator*()  const noexcept { return *Get(); }

        // =============================================================================
        // Get - Set
    public:
        /***/
        ResourceHandle<T> GetHandle() const noexcept { return m_Handle; }
        /***/
        bool              IsValid()   const noexcept { return m_Manager != nullptr && m_Handle.IsValid(); }
        /***/
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
