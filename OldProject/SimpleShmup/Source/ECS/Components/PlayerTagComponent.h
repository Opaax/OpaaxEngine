#pragma once
#include "Core/Component/OpaaxComponent.h"

/**
 * @struct PlayerTagComponent
 *
 * Empty marker. Entities carrying this are the player ship — read by
 * PlayerControlSystem and consulted by ShmupGameRulesSystem for collision filtering.
 */
struct PlayerTagComponent : public Opaax::OpaaxComponentBase
{
};
