#include "FontFaceResource.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IEngine.h"
#include "Core/IO/FileIO.h"
#include "Core/String/OpaaxPathString.h"
#include "Renderer/Text/FontBake.h"

namespace Opaax
{
    namespace
    {
        /** Single-channel coverage — the R8 branch OpenGLTexture2D::Upload already carries. */
        constexpr Int32 ATLAS_CHANNELS = 1;

        /**
         * The placeholder's vertical rhythm. Invented, because there is no font to ask — but not
         * arbitrary: a face with a zero LineAdvance would stack every line of tofu on top of itself,
         * and one with a zero PixelHeight would divide by zero the moment something scaled it.
         */
        constexpr float PLACEHOLDER_ASCENT_RATIO  = 0.8f;
        constexpr float PLACEHOLDER_DESCENT_RATIO = -0.2f;
    }

    std::optional<FontFaceResource> FontFaceResource::Load(const char* InPath, LoadContext& /*InCtx*/)
    {
        TDynArray<Uint8> lFileBytes;
        if (!FileIO::ReadAllBytes(OpaaxString(InPath), lFileBytes))
        {
            OPAAX_LOG(LogFontFaceResource, Error, "cannot read '{}'", InPath);
            return std::nullopt;
        }

        FontFaceResource lResource;
        if (!FontBake::Bake(lFileBytes.data(), lFileBytes.size(), FontBake::BakeParams{},
                            lResource.Face, lResource.Pixels))
        {
            OPAAX_LOG(LogFontFaceResource, Error, "cannot bake '{}'", InPath);
            return std::nullopt;
        }

        // Named local: PathString::Stem answers a view into a temporary, and CStr() across a `;`
        // would dangle.
        const OpaaxString lStem = PathString::Stem(OpaaxStringView(InPath)).ToString();

        OPAAX_LOG(LogFontFaceResource, Info, "'{}' baked {} glyph(s), atlas {}x{}, {} kern pair(s)",
                  lStem.CStr(), lResource.Face.GlyphCount(),
                  lResource.Face.AtlasWidth, lResource.Face.AtlasHeight,
                  lResource.Face.Kerning.size());

        return lResource;
    }

    void FontFaceResource::Initialize()
    {
        if (Pixels.empty() || Gpu != nullptr)
        {
            return;
        }

        Gpu = OpaaxApplication::GetAppService<IEngine>().CreateTexture(Pixels.data(),
                                                                      Face.AtlasWidth, Face.AtlasHeight,
                                                                      ATLAS_CHANNELS);

        Pixels.clear();
        Pixels.shrink_to_fit();

        if (Gpu == nullptr)
        {
            OPAAX_LOG(LogFontFaceResource, Warn, "no device — {}x{} atlas baked but not uploaded",
                      Face.AtlasWidth, Face.AtlasHeight);
        }
    }

    FontFaceResource FontFaceResource::Placeholder()
    {
        const float lHeight = FontBake::BakeParams{}.PixelHeight;

        FontFaceResource lResource;
        lResource.Face.PixelHeight          = lHeight;
        lResource.Face.VMetrics.Ascent      = lHeight * PLACEHOLDER_ASCENT_RATIO;
        lResource.Face.VMetrics.Descent     = lHeight * PLACEHOLDER_DESCENT_RATIO;
        lResource.Face.VMetrics.LineAdvance = lHeight;

        // No glyphs and no atlas, deliberately: every codepoint misses, so the whole string draws as
        // tofu boxes rather than as nothing at all.
        return lResource;
    }

    Uint64 FontFaceResource::ByteSize() const noexcept
    {
        // From the DIMENSIONS, not from Pixels.size(): Initialize empties that vector, and a payload's
        // accounting must not change because the pump happened to run.
        return sizeof(FontFaceResource)
             + static_cast<Uint64>(Face.AtlasWidth) * static_cast<Uint64>(Face.AtlasHeight)
             + static_cast<Uint64>(Face.Glyphs.size())  * (sizeof(Uint32) + sizeof(FontGlyph))
             + static_cast<Uint64>(Face.Kerning.size()) * sizeof(FontKerningPair);
    }
}
