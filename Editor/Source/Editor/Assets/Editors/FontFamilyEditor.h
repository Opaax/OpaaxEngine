#pragma once

#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Editor/Assets/IAssetEditor.h"
#include "Renderer/Text/FontStyle.h"
#include "Resources/ResourceRef.hpp"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(FontFamilyEditor);

    class FontFamily;
    struct FontFaceResource;
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // FontFamilyEditor — a family as a matrix: script across, weight down, for one width and slant at a
    //   time. A filled cell selects its face, an empty one adds it. Below: the selected face's style and
    //   file (a style the family already has is refused), a sample of text in that face laid out as the
    //   viewport does, and what a style request resolves to.
    // =============================================================================
    class FontFamilyEditor final : public IAssetEditor
    {
    public:
        explicit FontFamilyEditor(EditorContext& InContext) : m_Context(InContext) {}

        /** Releases the sample's face (the window closes before the resources go). */
        ~FontFamilyEditor() override;

        //~Begin IAssetEditor interface
        void DrawContents(AssetDocument& InDocument) override;

        Vector2F GetDefaultSize() const override { return { 640.f, 680.f }; }
        //~End IAssetEditor interface

    private:
        /** Width and slant: which plane of the matrix is shown (script x weight). */
        void DrawPlaneSelectors();

        /** Script across, weight down. '*' has a face, '?' has none yet, '+' adds the style. */
        void DrawMatrix(FontFamily& InFamily);

        /** The selected face's style and file, and Remove Face. */
        void DrawSelectedEntry(FontFamily& InFamily);

        /** A sample string in the selected face, laid out by Text2D::Layout like the viewport. */
        void DrawSample(const FontFamily& InFamily);

        /**
         * What a TextComponent asking for a style gets: width, slant and weight fall back, the script
         * never does.
         */
        void DrawResolve(const FontFamily& InFamily);

        /** Keeps a claim on InPath's face, releasing the previous one. Empty releases. */
        void ClaimSampleFace(const OpaaxString& InPath);

        EditorContext& m_Context;

        /** Which plane of the matrix is shown. */
        EFontWidth m_PlaneWidth = EFontWidth::Normal;
        EFontSlant m_PlaneSlant = EFontSlant::Normal;

        /** The selected entry, or -1. */
        Int32 m_Selected = -1;

        /** The style DrawResolve asks for. */
        FontStyleKey m_Ask;

        OpaaxString m_SampleText = "Sphinx of black quartz,\njudge my vow. 0123";
        float       m_SampleSize = 48.f;

        ResourceRef<FontFaceResource> m_SampleFace;
        OpaaxString                   m_SampleFacePath;
    };
}
