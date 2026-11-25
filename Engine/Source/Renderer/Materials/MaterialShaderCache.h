#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/Log/Logger.h"
#include "RHI/Shader.h"

namespace Opaax
{
    class SpriteShader;

    inline constexpr LogCategory LogMaterialShader{"MaterialShader"};

    // =============================================================================
    // MaterialShaderCache — the materials' shaders, compiled once per file. A file that has no stages
    //   or does not compile is logged once (the compiler's own error is logged by the device) and
    //   then answers null: its sprites are drawn with the engine's sprite shader. Never retried until
    //   cleared. The shaders are made through a function, so a test gives its own.
    // =============================================================================
    class MaterialShaderCache
    {
    public:
        using CreateFunction = TFunction<TUniquePtr<IShader>(const ShaderDesc&)>;

        /** What renderer.info tells of one shader file. */
        struct Entry
        {
            OpaaxString Path;            // the asset path the material names
            bool        bValid = false;  // compiled and linked
            OpaaxString Error;           // why not: not loaded, no sections, or the compiler's text
        };

        explicit MaterialShaderCache(CreateFunction InCreate) : m_Create(Move(InCreate)) {}

        /**
         * The compiled shader of InPath, compiled from InAsset the first time.
         * @param InAsset The loaded file, or null when it did not load (then null, logged once)
         * @return Null when it cannot be drawn with: the engine's sprite shader is used
         */
        IShader* Resolve(const OpaaxString& InPath, const SpriteShader* InAsset);

        /** Every file asked for so far, in the order first asked. */
        TDynArray<Entry> GetEntries() const;

        /** Calls InBeforeDestroy with each compiled shader (to drop what uses it), then forgets them all. */
        void Clear(const TFunction<void(const IShader&)>& InBeforeDestroy);

    private:
        struct Slot
        {
            OpaaxString         Path;
            TUniquePtr<IShader> Shader;   // null when it cannot be drawn with
            OpaaxString         Error;    // why it cannot
        };

        CreateFunction  m_Create;
        TDynArray<Slot> m_Slots;
    };
}
