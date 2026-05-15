#pragma once
#include "Core/OpaaxTypes.h"
#include "Core/Component/OpaaxComponent.h"

/**
 * @struct ScoreComponent
 *
 * Lives on the single persistent score entity created in
 * SimpleShmupGame::OnInitialize. Survives scene transitions.
 */
struct ScoreComponent : public Opaax::OpaaxComponentBase
{
    Opaax::Uint32 Score = 0;

    Opaax::json Serialize() const override
    {
        return { { "score", Score } };
    }

protected:
    void DeserializeImplementation(const Opaax::json& Json) override
    {
        if (Json.contains("score"))
        {
            Score = Json["score"].get<Opaax::Uint32>();
        }
    }
};
