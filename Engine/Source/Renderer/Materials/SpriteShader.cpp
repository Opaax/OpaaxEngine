#include "Renderer/Materials/SpriteShader.h"

#include "Core/IO/FileIO.h"
#include "Renderer/ShaderSource.h"

namespace Opaax
{
    bool SpriteShader::LoadSource(const char* InAbsPath)
    {
        const OpaaxString lPath(InAbsPath);

        // ReadAllText cannot tell a missing file from an empty one.
        TDynArray<Uint8> lBytes;
        if (!FileIO::ReadAllBytes(lPath, lBytes))
        {
            OPAAX_LOG(LogSpriteShader, Error, "cannot read '{}'", InAbsPath);
            return false;
        }

        // A file without both sections logs an error and gives empty stages.
        const OpaaxString lText(reinterpret_cast<const char*>(lBytes.data()), static_cast<Uint32>(lBytes.size()));
        m_Desc           = ShaderSource::FromSource(lText, lPath);
        m_Desc.DebugName = lPath;

        // A refused declaration only loses its name in the editor: said once, the shader still loads.
        m_Params = ShaderParams::Parse(OpaaxStringView(lText));
        for (const OpaaxString& lError : m_Params.Errors)
        {
            OPAAX_LOG(LogSpriteShader, Error, "'{}' @param {}", InAbsPath, lError.CStr());
        }
        return true;
    }

    Uint64 SpriteShader::GetResourceSize() const
    {
        return GetClass().GetSize() + m_Desc.VertexSrc.GetLength() + m_Desc.FragmentSrc.GetLength();
    }
}
