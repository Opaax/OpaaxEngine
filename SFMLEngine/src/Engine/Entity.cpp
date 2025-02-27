#include "Entity.h"

Entity::Entity(size_t Id, const EntityTags Tag)
{
    m_id = Id;
    m_tag = Tag;
    bIsActive = true;
}

void Entity::Destroy()
{
    bIsActive = false;
}
