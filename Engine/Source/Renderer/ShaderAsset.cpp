#include "ShaderAsset.h"

#include "RHI/Shader.h"

namespace Opaax
{
    // =============================================================================
    // CTORS - DTOR
    // =============================================================================
    ShaderAsset::ShaderAsset(const char* InVertexSrc, const char* InFragmentSrc)
        : m_State(EAssetState::Loading)
        , m_Gpu(IShader::Create(InVertexSrc, InFragmentSrc))
    {
        m_State = m_Gpu ? EAssetState::Loaded : EAssetState::Failed;
    }

    ShaderAsset::~ShaderAsset() = default;

    // =============================================================================
    // Functions
    // =============================================================================
    void ShaderAsset::Bind()   const { if (m_Gpu) { m_Gpu->Bind();   } }
    void ShaderAsset::Unbind() const { if (m_Gpu) { m_Gpu->Unbind(); } }

    void ShaderAsset::SetInt     (const char* InName, Int32           InValue)                  { if (m_Gpu) { m_Gpu->SetInt     (InName, InValue);          } }
    void ShaderAsset::SetIntArray(const char* InName, const Int32*    InValues, Uint32 InCount) { if (m_Gpu) { m_Gpu->SetIntArray(InName, InValues, InCount);} }
    void ShaderAsset::SetFloat   (const char* InName, float           InValue)                  { if (m_Gpu) { m_Gpu->SetFloat   (InName, InValue);          } }
    void ShaderAsset::SetFloat2  (const char* InName, const Vector2F& InValue)                  { if (m_Gpu) { m_Gpu->SetFloat2  (InName, InValue);          } }
    void ShaderAsset::SetFloat3  (const char* InName, const Vector3F& InValue)                  { if (m_Gpu) { m_Gpu->SetFloat3  (InName, InValue);          } }
    void ShaderAsset::SetFloat4  (const char* InName, const Vector4F& InValue)                  { if (m_Gpu) { m_Gpu->SetFloat4  (InName, InValue);          } }
    void ShaderAsset::SetMat4    (const char* InName, const Matrix44F& InValue)                 { if (m_Gpu) { m_Gpu->SetMat4    (InName, InValue);          } }
}
