#pragma once

#include "Core/String/OpaaxStringJson.h"
#include "World/Behaviour/Behaviour.h"
#include "World/Entity/EntityRef.h"

namespace TestWorld
{
    // =============================================================================
    // EntityRefProbe — records the name of the entity its Target names when it starts ("" when it
    //   names none): a script then sees which entity a reference resolves to, after a copy or a save.
    // =============================================================================
    class EntityRefProbe : public Opaax::Behaviour
    {
        OPAAX_OBJECT(EntityRefProbe, Opaax::Behaviour)

    public:
        Opaax::EntityRef   Target;
        Opaax::OpaaxString Found;

        OPAAX_PROPERTIES(EntityRefProbe, OPAAX_PROP(Target), OPAAX_PROP(Found))

        void OnStart() override
        {
            Opaax::Entity lTarget = Resolve(Target);
            Found = lTarget.IsValid() ? lTarget.GetName() : Opaax::OpaaxString();
        }
    };
}
