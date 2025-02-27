#include "EntityManager.h"

#include <iostream>

//CTOR Default
EntityManager::EntityManager()
{
    
}

void EntityManager::Update()
{
    CheckAddEntities();
    RemoveDeadEntities();
}

std::shared_ptr<Entity> EntityManager::AddEntity(const EntityTags Tag)
{
    auto lNewEntity = TSharedPtr<Entity>(new Entity(m_totalEntities++, Tag));
    lNewEntity->AddComponent<CTransform>();
    m_toAdd.push_back(lNewEntity);
    return lNewEntity;
}

void EntityManager::CheckAddEntities()
{
    for (auto lSHPEntity : m_toAdd)
    {
        m_entities.push_back(lSHPEntity);
        m_entityMap[lSHPEntity->GetEntityTag()].push_back(lSHPEntity);
    }

    m_toAdd.clear();
}

void EntityManager::CheckRemoveEntityInVec(EntityVector& InVector)
{
    std::_Erase_remove_if(InVector, [](TSharedPtr<Entity> entity)
    {
        return !(entity->IsActive());
    });
}

void EntityManager::RemoveDeadEntities()
{
    CheckRemoveEntityInVec(m_entities);

    for (auto& lPair : m_entityMap)
    {
        CheckRemoveEntityInVec(lPair.second);
    }
}