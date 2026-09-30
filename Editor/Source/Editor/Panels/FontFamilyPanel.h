#pragma once

#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Editor/Panels/IEditorPanel.h"

#include "Engine/Subsystems/Resources/ResourceRef.hpp"
#include "Engine/Subsystems/Resources/Types/Font/FontFamilyData.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(FontFamilyPanel);

    struct FontFaceResource;
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // FontFamilyPanel — the font family editor, as a matrix: script across, weight down, for one
    //   (width, slant) at a time. A filled cell selects its entry; an empty one adds it.
    //   Entries are reflected (DrawProperties gives the fields); duplicate styles are checked by
    //   FamilyOps::CommitEntryEdit.
    // =============================================================================
    class FontFamilyPanel final : public IEditorPanel
    {
        // =============================================================================
        // Statics
        // =============================================================================
    public:
        OPAAX_EDITOR_PANEL_NAME(Font Family);

        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        explicit FontFamilyPanel(EditorContext& InContext);
        ~FontFamilyPanel() override;

        // =============================================================================
        // Copy delete
        // =============================================================================
        FontFamilyPanel(const FontFamilyPanel&)            = delete;
        FontFamilyPanel& operator=(const FontFamilyPanel&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /** Name, dirty marker, Save, and the count. */
        void DrawHeader(const FontFamilyData& InData);

        /** Width / slant selectors (which plane of the matrix is shown). */
        void DrawPlaneSelectors();

        /** Script across, weight down. A filled cell selects; an empty one adds. */
        void DrawMatrix(const FontFamilyData& InData);

        /** The selected entry's fields, with undo, committed through FamilyOps. */
        void DrawSelectedEntry(FontFamilyData& InData);

        /**
         * What a style request actually gives (the fallback ladder): width, slant and weight fall back,
         * the script never does. Asks the family like a TextComponent would and shows the answer.
         */
        void DrawResolve(const FontFamilyData& InData);

        /**
         * A sample string in the selected face, using Text2D::Layout (the same as the viewport).
         */
        void DrawSample(const FontFamilyData& InData);

        /** Keeps a claim on InPath's face, releasing the previous one. Empty releases. */
        void ClaimSampleFace(const OpaaxString& InPath);

        /** Index of the entry at InStyle, or -1. */
        Int32 FindEntry(const FontFamilyData& InData, const FontStyleKey& InStyle) const;

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        /** Nothing to acquire. */
        void Startup()     override {}

        /** Nothing the world render depends on. */
        void OnPreRender() override {}

        void DrawContents() override;

        /** Releases the sample's claim while the ResourceManager and the GL context are alive. */
        void Shutdown()    override;

        PanelWindowStyle GetWindowStyle() const override { return { { 620.f, 460.f } }; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorContext& m_Context;

        /** Which plane of the matrix is shown. */
        EFontWidth m_PlaneWidth = EFontWidth::Normal;
        EFontSlant m_PlaneSlant = EFontSlant::Normal;

        /** Highlighted entry. -1 = none. */
        Int32 m_Selected = -1;

        /** The style DrawResolve asks for. */
        FontStyleKey m_Ask;

        // =============================================================================
        // The sample
        // =============================================================================
        OpaaxString m_SampleText = "Sphinx of black quartz,\njudge my vow. 0123";
        float       m_SampleSize = 48.f;

        /** The sample's face. */
        ResourceRef<FontFaceResource> m_SampleFace;
        OpaaxString                   m_SampleFacePath;

        // =============================================================================
        // The open edit gesture: the entry as it was when the first field became active
        // =============================================================================
        FontFamilyEntry m_GestureBefore;
        Uint32          m_GestureIndex   = 0;
        bool            m_bGestureOpen   = false;
        bool            m_bWasItemActive = false;
    };
}
