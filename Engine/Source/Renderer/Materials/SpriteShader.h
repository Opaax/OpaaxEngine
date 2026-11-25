#pragma once

#include "Core/Log/Logger.h"
#include "Resources/Asset/Asset.h"
#include "RHI/Shader.h"   // ShaderDesc

namespace Opaax
{
    inline constexpr LogCategory LogSpriteShader{"SpriteShader"};

    // =============================================================================
    // SpriteShader — a game's sprite shader: a .glsl file in the engine's `#type vertex` /
    //   `#type fragment` form, read as text. A Material2D names one in its Shader field; the renderer
    //   compiles it and draws that material's sprites with it. A file without both sections loads
    //   empty (logged): its sprites are drawn with the engine's sprite shader.
    //   Start from Engine/Assets/Shaders/SpriteMaterial.glsl: same vertex inputs and bindings.
    // =============================================================================
    class SpriteShader final : public Asset
    {
        OPAAX_OBJECT(SpriteShader, Asset)
        OPAAX_ASSET_SOURCE(".glsl")

        // =========================================================================
        // Override
        // =========================================================================
        //~Begin Asset interface
    public:
        /** Reads the file and splits it into its stages. False only when the file cannot be read. */
        bool LoadSource(const char* InAbsPath) override;

        /** The source text it holds. */
        Uint64 GetResourceSize() const override;
        //~End Asset interface

        // =========================================================================
        // Source
        // =========================================================================
    public:
        /** The stages, named after the file (DebugName). Empty stages when the file has no sections. */
        const ShaderDesc& GetDesc() const noexcept { return m_Desc; }

        /** True when both stages were found: something to compile. */
        bool HasStages() const noexcept { return !m_Desc.VertexSrc.IsEmpty() && !m_Desc.FragmentSrc.IsEmpty(); }

        // =========================================================================
        // Members
        // =========================================================================
    private:
        ShaderDesc m_Desc;
    };
}
