#include "ShaderAsset.h"

#include "RHI/Shader.h"
#include "Core/Log/OpaaxLog.h"

#include <fstream>
#include <sstream>
#include <string>

namespace Opaax
{
    namespace
    {
        // Split a single-file shader into a ShaderDesc. Sections are delimited by a line whose
        // first token is `#type`, followed by `vertex` or `fragment` (`pixel` accepted as an
        // alias). Content before the first `#type` is ignored.
        ShaderDesc ParseShaderStages(const OpaaxString& InSource, const OpaaxString& InDebugName)
        {
            ShaderDesc lDesc;
            lDesc.DebugName = InDebugName;

            std::istringstream lStream(InSource.CStr());
            std::string        lLine;
            std::string        lVert;
            std::string        lFrag;
            int                lStage = -1; // 0 = vertex, 1 = fragment, -1 = none/unknown

            while (std::getline(lStream, lLine))
            {
                if (!lLine.empty() && lLine.back() == '\r') { lLine.pop_back(); } // CRLF tolerance

                const size_t lFirst = lLine.find_first_not_of(" \t");
                if (lFirst != std::string::npos && lLine.compare(lFirst, 5, "#type") == 0)
                {
                    const std::string lRest = lLine.substr(lFirst + 5);
                    if      (lRest.find("vertex")   != std::string::npos) { lStage = 0; }
                    else if (lRest.find("fragment") != std::string::npos ||
                             lRest.find("pixel")    != std::string::npos) { lStage = 1; }
                    else                                                  { lStage = -1; }
                    continue;
                }

                if      (lStage == 0) { lVert += lLine; lVert += '\n'; }
                else if (lStage == 1) { lFrag += lLine; lFrag += '\n'; }
            }

            lDesc.VertexSrc   = OpaaxString(lVert.c_str());
            lDesc.FragmentSrc = OpaaxString(lFrag.c_str());
            return lDesc;
        }
    }

    // =============================================================================
    // CTORS - DTOR
    // =============================================================================
    ShaderAsset::ShaderAsset(const OpaaxString& InSourcePath, OpaaxStringID InAssetID)
        : m_AssetID(InAssetID)
        , m_SourcePath(InSourcePath)
        , m_State(EAssetState::Loading)
    {
        const OpaaxString lPath = InSourcePath;
        std::ifstream lFile(lPath.CStr(), std::ios::binary);
        if (!lFile.is_open())
        {
            OPAAX_CORE_ERROR("ShaderAsset: cannot open shader file '{}'", lPath);
            m_State = EAssetState::Failed;
            return;
        }

        std::stringstream lRaw;
        lRaw << lFile.rdbuf();
        const OpaaxString lSource(lRaw.str().c_str());

        const OpaaxString lName = InAssetID.ToString();
        const ShaderDesc  lDesc = ParseShaderStages(lSource, lName);

        if (lDesc.VertexSrc.IsEmpty() || lDesc.FragmentSrc.IsEmpty())
        {
            OPAAX_CORE_ERROR("ShaderAsset: '{}' missing a vertex or fragment '#type' section.", lPath);
            m_State = EAssetState::Failed;
            return;
        }

        m_Gpu   = IShader::Create(lDesc);
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
