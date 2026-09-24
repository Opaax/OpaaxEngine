// CollisionChannelList.h
//
// The single source of truth for collision channels. Add one by inserting an
// OPAAX_COLLISION_CHANNEL(Name) line below — CollisionChannel.h re-includes this file with
// different macro definitions to expand the list into both the ECollisionChannel enum body
// and the parallel name table.
//
// A channel answers "WHAT is this collider" (its category), never "how does it react" — that
// is the collider's Mode (Solid | Overlap) and its response mask.
//
// The enum ordinal doubles as the filter category-bit index, so order is load-bearing:
// appending is free, reordering or removing renumbers every saved map. APPEND, DO NOT RESHUFFLE.
// The filter is 64-bit, so at most 64 channels may exist.
//
// Do NOT add #pragma once — this file is intentionally re-included.

OPAAX_COLLISION_CHANNEL(WorldStatic)
OPAAX_COLLISION_CHANNEL(WorldDynamic)
OPAAX_COLLISION_CHANNEL(Pawn)
OPAAX_COLLISION_CHANNEL(Projectile)
OPAAX_COLLISION_CHANNEL(Trigger)
