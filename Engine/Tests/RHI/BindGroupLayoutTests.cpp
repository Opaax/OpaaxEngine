// Suite: BindGroupLayout — the uniform binding points a bind group lists: where each one is,
// and that no point is listed twice. Plain data: no GL context needed.
#include <doctest.h>

#include "RHI/BindGroup.h"

using namespace Opaax;

TEST_CASE("BindGroupLayout: a listed binding point is found at its place")
{
    const BindGroupLayout lLayout{ { 1u, 3u }, 16u };

    CHECK(lLayout.FindUniformBuffer(1) == 0);
    CHECK(lLayout.FindUniformBuffer(3) == 1);
    CHECK(lLayout.TextureSlotCount == 16);
}

TEST_CASE("BindGroupLayout: a binding point that is not listed is not found")
{
    const BindGroupLayout lLayout{ { 1u, 3u }, 16u };

    CHECK(lLayout.FindUniformBuffer(2) == BindGroupLayout::NOT_FOUND);
    CHECK(lLayout.FindUniformBuffer(0) == BindGroupLayout::NOT_FOUND);
}

TEST_CASE("BindGroupLayout: a layout without uniform blocks finds none, and is valid")
{
    const BindGroupLayout lLayout{ {}, 2u };

    CHECK(lLayout.FindUniformBuffer(0) == BindGroupLayout::NOT_FOUND);
    CHECK(lLayout.IsValid());
}

TEST_CASE("BindGroupLayout: each point once is valid, a point twice is not")
{
    CHECK(BindGroupLayout{ { 1u, 3u, 2u }, 0u }.IsValid());
    CHECK_FALSE(BindGroupLayout{ { 1u, 3u, 1u }, 0u }.IsValid());
    CHECK_FALSE(BindGroupLayout{ { 4u, 4u }, 0u }.IsValid());
}
