#pragma once

#include "Assets/AssetHandle.hpp"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxMathTypes.h"
#include "Core/OpaaxStringID.hpp"

namespace Opaax::ECS
{
    // /**
    //  * @struct TransformComponent
    //  *
    //  * 2D only for now. Z is draw order (painter's algorithm).
    //  * No dirty flag here — the render system reads this every frame.
    //  *      If transform caching becomes a perf issue, add a dirty bit then.
    //  */
    // struct TransformComponent
    // {
    //     Vector2F Position = { 0.f, 0.f };
    //     Vector2F Scale    = { 1.f, 1.f };
    //     float    Rotation = 0.f;           // radians, CCW
    //     float    ZOrder   = 0.f;           // draw order — higher = drawn on top
    // };
    
    // /**
    //  * @class SpriteComponent
    //  *
    //  * Drives Renderer2D::DrawSprite().
    //  * UVMin/UVMax allow atlas sub-regions — full texture = {0,0} to {1,1}.
    //  */
    // struct SpriteComponent
    // {
    //     TextureHandle Texture;
    //     Vector4F      Color   = { 1.f, 1.f, 1.f, 1.f };
    //     Vector2F      UVMin   = { 0.f, 0.f };
    //     Vector2F      UVMax   = { 1.f, 1.f };
    //     bool          Visible = true;
    // };
    
    // /**
    //  * @class TagComponent
    //  *
    //  * Human-readable name for an entity. Used by editor hierarchy + debug logs.
    //  * OpaaxStringID — O(1) compare, no heap per entity.
    //  */
    // struct TagComponent
    // {
    //     OpaaxStringID Tag;
    //
    //     TagComponent() noexcept = default;
    //     explicit TagComponent(OpaaxStringID InTag) noexcept : Tag(InTag) {}
    //     explicit TagComponent(const char* InTag) : Tag(InTag) {}
    // };

} // namespace Opaax::ECS