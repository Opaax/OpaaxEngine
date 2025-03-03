#pragma once

#include <tuple>
#include "Component.hpp"
#include "EngineType.hpp"

using ComponentTuple = std::tuple<CTransform, CCircleShape, CPlayerInput, CLifeSpan, CCollision, CRender, CAnimation>;

//Use struct because all my tags are public by default, make more sense using struct.
enum struct EntityTags 
{
    none = -1,
    player,
    enemy,
    smallEnemy,
    bullet
};

class Entity
{
    //Make EntityMgr as friend to manage memory of Entities.
    friend class EntityManager;

private:
    ComponentTuple  m_components    {};
    size_t          m_id	        { 0 };
    EntityTags      m_tag           { EntityTags::none };
    bool            bIsActive       { true };
    
private:
    //Private CTOR, making sure to get entity through the EntityMgr
    Entity() = default;
    Entity(size_t Id, const EntityTags Tag);

public:
    void Destroy();
    
    EntityTags  GetEntityTag()  const {return m_tag;}
    size_t      GetID()         const {return m_id;}
    bool        IsActive()      const {return bIsActive;}

    //Templates func to:
    // - Add
    // - Remove
    // - Get, const Get
    // - Has
    // Components, for the ECS pattern.
    // All those template ensuring that template argument must inherit from Component.
    template<typename T, typename... TArgs, typename = TDerive<Component, T>>
    T& AddComponent(TArgs&&... Args);

    template <typename T, typename = TDerive<Component, T>>
    void RemoveComponent();

    template<typename T, typename = TDerive<Component, T>>
    T& GetComponent();

    template<typename T, typename = TDerive<Component, T>>
    bool HasComponent() const;

    STDString ToString() const
    {
        STDString lIDStr = "Entity: " + std::to_string(m_id);
        STDString lTypeStr;

        switch (m_tag) {
        case EntityTags::none:
            lTypeStr = "None";
            break;
        case EntityTags::player:
            lTypeStr = "Player";
            break;
        case EntityTags::enemy:
            lTypeStr = "Enemy";
            break;
        case EntityTags::smallEnemy:
            lTypeStr = "Small Enemy";
            break;
        case EntityTags::bullet:
            lTypeStr = "Bullet";
            break;
        }
        
        return lIDStr + " type of: " + lTypeStr;
    }
};

template <typename T, typename ... TArgs, typename>
T& Entity::AddComponent(TArgs&&... Args)
{
    auto& lComp = std::get<T>(m_components);
    lComp = T(std::forward<TArgs>(Args)...);
    lComp.SetExisting(true);
    return lComp;
}

template <typename T, typename>
void Entity::RemoveComponent()
{
    return std::get<T>(m_components) = T();
}

template <typename T, typename>
T& Entity::GetComponent()
{
    return std::get<T>(m_components);
}

template <typename T, typename>
bool Entity::HasComponent() const
{
    return std::get<T>(m_components).IsExisting();
}