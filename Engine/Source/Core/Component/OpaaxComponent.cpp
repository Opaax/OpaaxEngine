#include "OpaaxComponent.h"

#include "Core/Log/OpaaxLog.h"

Opaax::json Opaax::OpaaxComponentBase::Serialize() const
{
    OPAAX_CORE_WARN("OpaaxComponentBase::Serialize(), Not implemented");
    return IOpaaxComponent::Serialize();
}

void Opaax::OpaaxComponentBase::DeserializeImplementation(const json& Json)
{
    OPAAX_CORE_WARN("OpaaxComponentBase::Deserialize(), Not implemented");
}
