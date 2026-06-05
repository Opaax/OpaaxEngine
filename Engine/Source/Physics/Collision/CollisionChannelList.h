// CollisionChannelList.h
//
// Single source of truth for collision channels (object-type categories used for
// physics filtering). Add a new channel by inserting one OPAAX_COLLISION_CHANNEL(Name)
// line below — the consumer (CollisionChannel.h) re-includes this file with different
// macro definitions to expand the list into both the ECollisionChannel enum body and
// the parallel g_CollisionChannelIDs string array.
//
// A channel answers "WHAT is this collider" (its category), NOT "how does it interact"
// (that is the collider's Mode = Solid|Trigger and the CollisionProfile response matrix).
// The enum ordinal doubles as the Box2D category-bit index, so order is stable: appending
// is free, reordering/removing renumbers existing scenes — append, do not reshuffle.
//
// Box2D's filter category/mask is 64-bit, so at most 64 channels may exist.
//
// Do NOT add #pragma once — this file is intentionally re-included.

OPAAX_COLLISION_CHANNEL(WorldStatic)
OPAAX_COLLISION_CHANNEL(WorldDynamic)
OPAAX_COLLISION_CHANNEL(Pawn)
OPAAX_COLLISION_CHANNEL(Projectile)
OPAAX_COLLISION_CHANNEL(Trigger)
