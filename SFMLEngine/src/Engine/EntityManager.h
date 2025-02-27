#pragma once

#include <map>
#include <memory>
#include "EngineType.hpp"
#include "Entity.h"

using EntityVector = std::vector<std::shared_ptr<Entity>>;
using EntityMap = std::map<EntityTags, EntityVector>;

class EntityManager
{
public:
private:
    EntityVector m_entities;
    EntityMap m_entityMap;
    EntityVector m_toAdd;
    size_t    m_totalEntities{ 0 };

    void CheckAddEntities();
    void CheckRemoveEntityInVec(EntityVector& InVector);
    void RemoveDeadEntities();

public:
    EntityManager();

    void Update();

    TSharedPtr<Entity> AddEntity(const EntityTags Tag);

    const EntityVector& GetEntities() const {return m_entities;}
    const EntityVector& GetEntities(const EntityTags Tag) {return m_entityMap[Tag];}

    const EntityMap& GetEntitiesMap() const {return m_entityMap;}
};
