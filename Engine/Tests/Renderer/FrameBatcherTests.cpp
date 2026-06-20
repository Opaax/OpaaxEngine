// Suite: frame batch emitter (Renderer/FrameBatcher.h).
//
// AssignBatches is header-inline + pure (no RHI, opaque Uint64 texture identities), so this suite
// compiles the function directly and feeds it synthetic command lists. It pins the contract the
// frame-global renderer relies on: batches never exceed the quad/slot caps, concatenating them in
// index order preserves the input draw order, slot maps are consistent per batch, and white (0)
// always lands in slot 0. Draw calls == batch count and quads == command count fall out of the same
// numbers (the render-stats derivation).
#include <doctest.h>

#include "Renderer/FrameBatcher.h"

using namespace Opaax;

TEST_CASE("AssignBatches: empty input emits zero batches")
{
    CHECK(AssignBatches(nullptr, 0, 1000, 16, nullptr) == 0u);
}

TEST_CASE("AssignBatches: white-only commands share one batch, all slot 0")
{
    const Uint64    lKeys[3] = { 0, 0, 0 };
    BatchAssignment lOut[3];

    const Uint32 lBatches = AssignBatches(lKeys, 3, 1000, 16, lOut);

    CHECK(lBatches == 1u);
    for (int i = 0; i < 3; ++i)
    {
        CHECK(lOut[i].BatchIndex == 0u);
        CHECK(lOut[i].Slot       == 0u);   // white never consumes a user slot
    }
}

TEST_CASE("AssignBatches: quad cap splits batches, draw calls == ceil(n / cap)")
{
    // 5 same-texture commands, cap 2 -> 3 batches. Same texture keeps slot 1 in every batch.
    const Uint64    lKeys[5] = { 0x1000, 0x1000, 0x1000, 0x1000, 0x1000 };
    BatchAssignment lOut[5];

    const Uint32 lBatches = AssignBatches(lKeys, 5, /*maxQuads*/2, /*maxSlots*/16, lOut);

    CHECK(lBatches == 3u);                  // == DrawCalls for this synthetic frame
    CHECK(lOut[0].BatchIndex == 0u); CHECK(lOut[1].BatchIndex == 0u);
    CHECK(lOut[2].BatchIndex == 1u); CHECK(lOut[3].BatchIndex == 1u);
    CHECK(lOut[4].BatchIndex == 2u);
    CHECK(lOut[0].Slot == 1u); CHECK(lOut[2].Slot == 1u); CHECK(lOut[4].Slot == 1u);
}

TEST_CASE("AssignBatches: slot cap splits batches, each batch <= maxSlots distinct keys")
{
    // maxSlots 3 = white + 2 user textures. Four distinct textures -> two batches of two.
    const Uint64    lKeys[4] = { 0xA, 0xB, 0xC, 0xD };
    BatchAssignment lOut[4];

    const Uint32 lBatches = AssignBatches(lKeys, 4, /*maxQuads*/1000, /*maxSlots*/3, lOut);

    CHECK(lBatches == 2u);
    CHECK(lOut[0].BatchIndex == 0u); CHECK(lOut[0].Slot == 1u);  // A
    CHECK(lOut[1].BatchIndex == 0u); CHECK(lOut[1].Slot == 2u);  // B
    CHECK(lOut[2].BatchIndex == 1u); CHECK(lOut[2].Slot == 1u);  // C (fresh batch, slots reset)
    CHECK(lOut[3].BatchIndex == 1u); CHECK(lOut[3].Slot == 2u);  // D
}

TEST_CASE("AssignBatches: same key reuses its slot; white stays 0 amid textures")
{
    // A, white, A, B, white, A — one batch, slots stable per key.
    const Uint64    lKeys[6] = { 0xA, 0, 0xA, 0xB, 0, 0xA };
    BatchAssignment lOut[6];

    const Uint32 lBatches = AssignBatches(lKeys, 6, 1000, 16, lOut);

    CHECK(lBatches == 1u);
    CHECK(lOut[0].Slot == 1u);   // A
    CHECK(lOut[1].Slot == 0u);   // white
    CHECK(lOut[2].Slot == 1u);   // A reuses slot 1
    CHECK(lOut[3].Slot == 2u);   // B
    CHECK(lOut[4].Slot == 0u);   // white
    CHECK(lOut[5].Slot == 1u);   // A still slot 1
}

TEST_CASE("AssignBatches: batch indices are non-decreasing (concatenation preserves draw order)")
{
    // Tight caps to force many splits; the walk must never send a later command to an earlier batch.
    const Uint64    lKeys[8] = { 0xA, 0xB, 0xC, 0xA, 0xD, 0xB, 0xE, 0xF };
    BatchAssignment lOut[8];

    const Uint32 lBatches = AssignBatches(lKeys, 8, /*maxQuads*/3, /*maxSlots*/3, lOut);

    CHECK(lBatches >= 1u);
    for (int i = 1; i < 8; ++i)
    {
        CHECK(lOut[i].BatchIndex >= lOut[i - 1].BatchIndex);
        // never exceed maxQuads - 1 quads behind in the same batch is implied; slot stays in range
        CHECK(lOut[i].Slot < 3u);
    }
}
