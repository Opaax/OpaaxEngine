#pragma once
#include "Core/Component/OpaaxComponent.h"

/**
 * @struct EnemyTagComponent
 *
 * Empty marker. Entities carrying this are enemy ships — spawned by
 * EnemySpawnSystem and counted as collision targets by ShmupGameRulesSystem.
 */
struct EnemyTagComponent : public Opaax::OpaaxComponentBase 
{};
