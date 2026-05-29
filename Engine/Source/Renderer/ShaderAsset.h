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
         * Compile + link a vertex/fragment shader pair. Asset ID empty (runtime-built),
         * source path empty. State ends as Loaded; OpenGLShader asserts on
         * compile/link failure rather than reporting through state.
         */
        ShaderAsset(const char* InVertexSrc, const char* InFragmentSrc);

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
