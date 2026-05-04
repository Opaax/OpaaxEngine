#pragma once
#include "Assets/AssetHandle.hpp"
#include "Core/OpaaxMathTypes.h"
#include "Core/Component/OpaaxComponent.h"

namespace Opaax::ECS
{
    using json = nlohmann::json;
    
    /**
         * @class SpriteComponent
         *
         * Drives Renderer2D::DrawSprite().
         * UVMin/UVMax allow atlas sub-regions — full texture = {0,0} to {1,1}.
         * Size is the intrinsic world-space size; final draw size = Size * WorldTransform.Scale.
         * Decoupling Size from Transform.Scale lets parent scaling compose multiplicatively
         * without forcing per-sprite Scale hacks.
         */
    struct OPAAX_API SpriteComponent : public OpaaxComponentBase
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        SpriteComponent()             = default;

        SpriteComponent(const TextureHandle& InTexture) : Texture(InTexture) {}

        SpriteComponent(const TextureHandle& InTexture, Vector2F InSize)
        : Texture(InTexture), Size(InSize) {}

        SpriteComponent(const TextureHandle& InTexture, Vector2F InUVMin, Vector2F InUVMax)
        : Texture(InTexture), UVMin(InUVMin), UVMax(InUVMax) {}

        explicit SpriteComponent(const json& Json) : OpaaxComponentBase(Json){ Deserialize(Json); }

        virtual ~SpriteComponent()    = default;
        
        // =============================================================================
        // Override
        // =============================================================================
        //~ Begin OpaaxComponentBase Interface
    protected:
        void DeserializeImplementation(const json& Json) override;
        
    public:
        json Serialize() const override;
        //~ End OpaaxComponentBase Interface

        // =============================================================================
        // Members
        // =============================================================================
        TextureHandle Texture;
        Vector2F      Size    = { 1.f, 1.f };
        Vector4F      Color   = { 1.f, 1.f, 1.f, 1.f };
        Vector2F      UVMin   = { 0.f, 0.f };
        Vector2F      UVMax   = { 1.f, 1.f };
        bool          Visible = true;
    };
}
