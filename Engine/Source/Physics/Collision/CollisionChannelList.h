// CollisionChannelList.h — the list of collision channels. Add one OPAAX_COLLISION_CHANNEL(Name) line.
// CollisionChannel.h includes this file several times (no #pragma once).
// A channel says what a collider is; its Mode says how it reacts.
// The order is the filter bit index: append only (reordering breaks saved maps). At most 64.

OPAAX_COLLISION_CHANNEL(WorldStatic)
OPAAX_COLLISION_CHANNEL(WorldDynamic)
OPAAX_COLLISION_CHANNEL(Pawn)
OPAAX_COLLISION_CHANNEL(Projectile)
OPAAX_COLLISION_CHANNEL(Trigger)
