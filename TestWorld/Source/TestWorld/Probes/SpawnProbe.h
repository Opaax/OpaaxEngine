#pragma once

#include "Core/String/OpaaxStringJson.h"
#include "World/Behaviour/Behaviour.h"

namespace TestWorld
{
    // =============================================================================
    // SpawnProbe — spawns Count instances of Prefab in a row, creates a plain entity named
    //   "ProbeCreated", then destroys the last instance DestroyAfter seconds later.
    // =============================================================================
    class SpawnProbe : public Opaax::Behaviour
    {
    public:
        Opaax::OpaaxString Prefab       = "Prefabs/Crate.opaaxprefab";
        Opaax::Int32       Count        = 3;
        float              Spacing      = 80.f;
        float              DestroyAfter = 0.2f;

        Opaax::Int32 Spawned       = 0;
        bool         bCreated      = false;
        bool         bDestroyedOne = false;

        OPAAX_PROPERTIES(SpawnProbe, OPAAX_PROP(Prefab), OPAAX_PROP(Count), OPAAX_PROP(Spacing),
                         OPAAX_PROP(DestroyAfter), OPAAX_PROP(Spawned), OPAAX_PROP(bCreated), OPAAX_PROP(bDestroyedOne))

        void OnStart() override
        {
            const Opaax::Vector2F lOrigin = GetTransform().Position;
            for (Opaax::Int32 lIndex = 0; lIndex < Count; ++lIndex)
            {
                const Opaax::Entity lInstance =
                    Spawn(Prefab, lOrigin + Opaax::Vector2F{ Spacing * static_cast<float>(lIndex), 0.f });
                if (lInstance.IsValid())
                {
                    ++Spawned;
                    m_Last = lInstance;
                }
            }

            bCreated = CreateEntity(Opaax::OpaaxString("ProbeCreated")).IsValid();
            SetTimer<&SpawnProbe::DestroyLast>(DestroyAfter);
        }

        void DestroyLast()
        {
            if (m_Last.IsValid())
            {
                Destroy(m_Last);
                bDestroyedOne = true;
            }
        }

    private:
        Opaax::Entity m_Last;
    };
}
