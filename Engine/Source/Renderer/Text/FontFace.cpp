#include "Renderer/Text/FontFace.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IEngine.h"
#include "Core/IO/FileIO.h"
#include "Renderer/Text/FontBake.h"

namespace Opaax
{
    namespace
    {
        /** Single-channel (R8) coverage. */
        constexpr Int32 ATLAS_CHANNELS = 1;
    }

    FontFace::FontFace()  = default;
    FontFace::~FontFace() = default;

    bool FontFace::LoadSource(const char* InAbsPath)
    {
        TDynArray<Uint8> lFileBytes;
        if (!FileIO::ReadAllBytes(OpaaxString(InAbsPath), lFileBytes))
        {
            OPAAX_LOG(LogFontFace, Error, "cannot read '{}'", InAbsPath);
            return false;
        }

        if (!FontBake::Bake(lFileBytes.data(), lFileBytes.size(), FontBake::BakeParams{}, m_Data, m_Pixels))
        {
            OPAAX_LOG(LogFontFace, Error, "cannot bake '{}'", InAbsPath);
            return false;
        }

        OPAAX_LOG(LogFontFace, Trace, "'{}' baked {} glyph(s), atlas {}x{}, {} kern pair(s)",
                  GetName().CStr(), m_Data.GlyphCount(), m_Data.AtlasWidth, m_Data.AtlasHeight,
                  m_Data.Kerning.size());

        return true;
    }

    void FontFace::InitializeOnMainThread()
    {
        if (m_Pixels.empty() || m_Gpu != nullptr)
        {
            return;
        }

        m_Gpu = OpaaxApplication::GetAppService<IEngine>().CreateTexture(m_Pixels.data(), m_Data.AtlasWidth,
                                                                        m_Data.AtlasHeight, ATLAS_CHANNELS);

        // Free the CPU copy: the GPU has the atlas, or nothing can upload it.
        m_Pixels.clear();
        m_Pixels.shrink_to_fit();

        if (m_Gpu == nullptr)
        {
            OPAAX_LOG(LogFontFace, Warn, "no device — '{}' ({}x{} atlas) baked but not uploaded",
                      GetName().CStr(), m_Data.AtlasWidth, m_Data.AtlasHeight);
        }
    }

    Uint64 FontFace::GetResourceSize() const
    {
        // From the dimensions: the upload frees the pixels.
        return sizeof(FontFace)
             + static_cast<Uint64>(m_Data.AtlasWidth) * static_cast<Uint64>(m_Data.AtlasHeight)
             + static_cast<Uint64>(m_Data.Glyphs.size())  * (sizeof(Uint32) + sizeof(FontGlyph))
             + static_cast<Uint64>(m_Data.Kerning.size()) * sizeof(FontKerningPair);
    }
}
