#pragma once
#include "Core/Component/OpaaxComponent.h"

/**
 * @struct BulletTagComponent
 *
 * Empty marker. Entities carrying this are player-fired bullets — spawned by
 * PlayerControlSystem; only player bullets exist in the G1 slice.
 */
struct BulletTagComponent : public Opaax::OpaaxComponentBase
{
};
