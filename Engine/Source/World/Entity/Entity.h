#pragma once

#include "Core/EngineAPI.h"
#include "World/Entity/EntityTypes.h"
#include "World/Entity/EntityMeta.h"
#include "Core/GUID/Guid.h"
#include "World/World.h"

namespace Opaax
{
    // =============================================================================
    // Entity — a light handle to an entity in a World (EntityID + World*). Cheap to copy.
    //   A default Entity is invalid.
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
         * Adds a component T.
         * @return The added component
         */
        template<typename T, typename... Args>
        T& Add(Args&&... InArgs)
        {
            return m_World->GetRegistry().emplace<T>(m_Handle, std::forward<Args>(InArgs)...);
        }

        /**
         * Adds or replaces a component T.
         * @return The component
         */
        template<typename T, typename... Args>
        T& AddOrReplace(Args&&... InArgs)
        {
            return m_World->GetRegistry().emplace_or_replace<T>(m_Handle, std::forward<Args>(InArgs)...);
        }

        /**
         * @return The component T (must exist)
         */
        template<typename T> 
        T& Get() { return m_World->GetRegistry().get<T>(m_Handle); }
        
        /**
         * @return The component T, or nullptr
         */
        template<typename T> 
        T* TryGet() { return m_World->GetRegistry().try_get<T>(m_Handle); }
        
        /**
         * @return True if the entity has a component T
         */
        template<typename T> 
        bool Has() const  { return m_World->GetRegistry().all_of<T>(m_Handle); }
        
        /**
         * Removes the component T.
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
