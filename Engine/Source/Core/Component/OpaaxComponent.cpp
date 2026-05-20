#include "OpaaxComponent.h"

Opaax::json Opaax::OpaaxComponentBase::Serialize() const
{
    return IOpaaxComponent::Serialize();
}

void Opaax::OpaaxComponentBase::DeserializeImplementation(const json& Json)
{
}
