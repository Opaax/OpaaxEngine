// ShakeDecayList.h
//
// Single source of truth for shake-decay curves. Add a new curve by
// inserting one OPAAX_SHAKE_DECAY(Name) line below — the consumer
// (ShakeParams.h) re-includes this file with different macro definitions
// to expand the list into both the EShakeDecay enum body and the parallel
// g_ShakeDecayIDs string array.
//
// Do NOT add #pragma once — this file is intentionally re-included.

OPAAX_SHAKE_DECAY(Linear)
OPAAX_SHAKE_DECAY(EaseOut)
OPAAX_SHAKE_DECAY(Constant)
