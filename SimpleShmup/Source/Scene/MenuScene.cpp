#include "MenuScene.h"

#include "Core/Log/OpaaxLog.h"
#include "World/World.h"

#include "ECS/Components/ScoreComponent.h"

void MenuScene::OnLoad(Opaax::World& InWorld)
{
    OPAAX_TRACE("[MenuScene] OnLoad — press Space to start");

    Opaax::Uint32 lScore = 0;
    InWorld.Each<ScoreComponent>(
        [&lScore](Opaax::EntityID, const ScoreComponent& InScore)
        {
            lScore = InScore.Score;
        });

    OPAAX_INFO("[MenuScene] Score on entry: {}", lScore);
}
