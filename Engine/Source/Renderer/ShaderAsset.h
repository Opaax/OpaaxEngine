#pragma once

#include "Assets/IAsset.hpp"
#include "Core/EngineAPI.h"
#include "Core/OpaaxMathTypes.h"
#include "Core/OpaaxString.hpp"
#include "Core/OpaaxStringID.hpp"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    class IShader;

    // =============================================================================
    // ShaderAsset
    // =============================================================================
    /**
     * @class ShaderAsset
     *
     * Logical shader asset. Composes a UniquePtr<IShader> as the GPU resource —
     * the IAsset surface lives here, the backend program object stays inside the
     * concrete IShader impl (OpenGLShader today). Never names a backend type.
     *
     * Shaders today are built from inline string literals at engine init
     * (Renderer2D); no on-disk shader files exist yet, so there is no
     * IAssetLoader<ShaderAsset>. The IAsset surface is in place so future
     * disk-loaded shaders can plug into AssetRegistry without changing this class.
     *
     * Lifetime: heap-owned by whoever constructs it (Renderer2D for now).
     * Move/copy deleted — the GL program inside OpenGLShader is RAII-tied.
     */
    class OPAAX_API ShaderAsset final : public IAsset
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        /**
         * Load a shader from a single on-disk file containing `#type vertex` / `#type fragment`
         * sections (GLSL today). The ctor reads + splits the file into a ShaderDesc and builds
         * the backend IShader via IShader::Create. State ends Loaded on success, Failed if the
         * file is missing, a stage is empty, or GPU compile/link fails (fail-loud, logged).
         * @param InSourcePath Path to the .glsl file (project/engine-relative; stored verbatim).
         * @param InAssetID    Registry-stable logical ID.
         */
        ShaderAsset(const OpaaxString& InSourcePath, OpaaxStringID InAssetID);

        ~ShaderAsset() override;

        // =============================================================================
        // Copy - delete
        // =============================================================================
        ShaderAsset(const ShaderAsset&)            = delete;
        ShaderAsset& operator=(const ShaderAsset&) = delete;

        // =============================================================================
        // Move - delete
        // =============================================================================
        ShaderAsset(ShaderAsset&&)                 = delete;
        ShaderAsset& operator=(ShaderAsset&&)      = delete;

        // =============================================================================
        // Functions
        // =============================================================================
        //~ Begin IAsset interface
    public:
        OpaaxStringID      GetAssetID()    const override { return m_AssetID;    }
        AssetType          GetType()       const override { return AssetType::Shader; }
        EAssetState        GetState()      const override { return m_State;      }
        const OpaaxString& GetSourcePath() const override { return m_SourcePath; }
        //~ End IAsset interface

    public:
        bool IsLoaded() const noexcept { return m_State == EAssetState::Loaded; }

        void Bind()   const;
        void Unbind() const;

        //------------------------------------------------------------------------------
        //  Uniform setters

        void SetInt     (const char* InName, Int32           InValue);
        void SetIntArray(const char* InName, const Int32*    InValues, Uint32 InCount);
        void SetFloat   (const char* InName, float           InValue);
        void SetFloat2  (const char* InName, const Vector2F& InValue);
        void SetFloat3  (const char* InName, const Vector3F& InValue);
        void SetFloat4  (const char* InName, const Vector4F& InValue);
        void SetMat4    (const char* InName, const Matrix44F& InValue);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpaaxStringID            m_AssetID    = {};
        OpaaxString              m_SourcePath;
        EAssetState              m_State      = EAssetState::Unloaded;
        UniquePtr<IShader>       m_Gpu;
    };

} // namespace Opaax
