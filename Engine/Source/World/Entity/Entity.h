#pragma once

#include "Core/EngineAPI.h"
#include "World/Entity/EntityTypes.h"
#include "World/Entity/EntityMeta.h"
#include "GUID/Guid.h"
#include "World/World.h"

namespace Opaax
{
    // =============================================================================
    // Entity — a lightweight handle to one entity in a World: an (EntityID, World*)
    //   pair, copyable and cheap. Every operation routes through the owning World's
    //   registry, so raw entt never escapes the World layer. A default / null-handle
    //   Entity is "invalid".
    // =============================================================================
    class Entity
    {
        // =========================================================================
        // CTORS
        // =========================================================================
    public:
        Entity() = default;
        Entity(EntityID InHandle, World* InWorld) : m_Handle(InHandle), m_World(InWorld) {}

        // =========================================================================
        // Components
    public:
        /**
         * @tparam T 
         * @tparam Args 
         * @param InArgs 
         * @return The Component add to this entity
         */
        template<typename T, typename... Args>
        T& Add(Args&&... InArgs)
        {
            return m_World->GetRegistry().emplace<T>(m_Handle, std::forward<Args>(InArgs)...);
        }

        /**
         * @tparam T 
         * @tparam Args 
         * @param InArgs 
         * @return The new Component add or replaced
         */
        template<typename T, typename... Args>
        T& AddOrReplace(Args&&... InArgs)
        {
            return m_World->GetRegistry().emplace_or_replace<T>(m_Handle, std::forward<Args>(InArgs)...);
        }

        /**
         * @tparam T 
         * @return A ref of the component type T
         */
        template<typename T> 
        T& Get() { return m_World->GetRegistry().get<T>(m_Handle); }
        
        /**
         * @tparam T 
         * @return A Ptr of the component type T
         */
        template<typename T> 
        T* TryGet() { return m_World->GetRegistry().try_get<T>(m_Handle); }
        
        /**
         * @tparam T Component Type
         * @return True if has the component of type T
         */
        template<typename T> 
        bool Has() const  { return m_World->GetRegistry().all_of<T>(m_Handle); }
        
        /**
         * @tparam T The component type to remove
         */
        template<typename T> 
        void Remove() { m_World->GetRegistry().remove<T>(m_Handle); }
        
        // End Components
        // =========================================================================

        // =========================================================================
        // Identity / lifecycle
    public:
        Guid GetGuid() const
        {
            if (const EntityMeta* lMeta = m_World->GetRegistry().try_get<EntityMeta>(m_Handle))
            {
                return lMeta->Id;
            }
            return Guid{};
        }

        bool IsValid() const noexcept { return m_World != nullptr && m_World->IsValid(m_Handle); }
        explicit operator bool() const noexcept { return IsValid(); }

        void Destroy()
        {
            if (m_World != nullptr)
            {
                m_World->DestroyEntity(m_Handle);
            }
        }

        EntityID GetHandle() const noexcept { return m_Handle; }
        World*   GetWorld()  const noexcept { return m_World; }
        
        // End Identity / lifecycle
        // =========================================================================

        // =========================================================================
        // Members
        // =========================================================================
    private:
        EntityID m_Handle = ENTITY_NONE;
        World*   m_World   = nullptr;
    };
}
