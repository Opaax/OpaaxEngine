#pragma once

#include "Application/Services/ILogger.h"
#include "Core/OpaaxTypes.h"
#include "Editor/Panels/IEditorPanel.h"

#include "Engine/Subsystems/Resources/ResourceRef.hpp"          // one claim for the sample's face
#include "Engine/Subsystems/Resources/Types/Font/FontFamilyData.h"   // the gesture caches an entry

namespace Opaax
{
    OPAAX_LOG_CATEGORY(FontFamilyPanel);

    struct FontFaceResource;   // only NAMED by the held claim
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // FontFamilyPanel — the family EDITOR: which cuts of a typeface exist, as a MATRIX.
    //
    //   A LIST WOULD BE UNUSABLE HERE, and that is the one thing this panel does differently from
    //   AnimationLibraryPanel, whose shape it otherwise copies. Roboto has 162 entries; scrolling
    //   162 rows to answer "do I have Greek Bold Italic?" is not an answer. So the table is the
    //   question the family is actually asked: **subset across, weight down**, for one (width,
    //   slant) at a time — which is 81 cells, one screen, and reads as coverage.
    //
    //   A cell is a VERB, not a label: filled selects that entry, empty adds one at that style. That
    //   is what makes the matrix an editor rather than a report, and it is why there is no "Add"
    //   button — an add always has a style, and the cell already knows which.
    //
    //   An entry is CReflected, so DrawProperties gives the four dropdowns AND the typed drop target
    //   with no drawer written here. What it cannot give is judgement about the REST of the family —
    //   a duplicate style makes one face unreachable — so the gesture closes through
    //   FamilyOps::CommitEntryEdit, which is where that policy lives.
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

        /** The width / slant selectors that choose WHICH plane of the matrix is on screen. */
        void DrawPlaneSelectors();

        /** Subset across, weight down. A filled cell selects; an empty one adds. */
        void DrawMatrix(const FontFamilyData& InData);

        /** The selected entry's two fields, bracketed for undo and committed through FamilyOps. */
        void DrawSelectedEntry(FontFamilyData& InData);

        /**
         * What a style request actually answers — the FALLBACK LADDER, spelled out where the family
         * is authored.
         *
         * **TX6** is invisible otherwise: width, slant and weight all fall back and the subset never
         * does, so a family missing Greek Bold quietly draws Greek Regular while a family missing
         * Greek entirely draws boxes. Both are correct and neither is discoverable by looking at a
         * matrix of the cuts that DO exist — you find out at runtime, in the viewport, on the wrong
         * day. This asks the family the same question a TextComponent asks it, and prints the answer.
         */
        void DrawResolve(const FontFamilyData& InData);

        /**
         * The selected face, drawing a sample string with its OWN glyphs — Windows' font viewer, in
         * the panel where a family is chosen.
         *
         * It goes through `Text2D::Layout`, the same walk `DrawString` uses, with an ImGui sink
         * instead of a Renderer2D one. Re-implementing the layout here is what would let the preview
         * and the viewport disagree, which would make the preview worse than nothing (**TX5**).
         */
        void DrawSample(const FontFamilyData& InData);

        /** Keep a claim on InPath's face, releasing the previous one. Empty releases and holds none. */
        void ClaimSampleFace(const OpaaxString& InPath);

        /** Index of the entry at InStyle, or -1. The matrix asks this once per cell. */
        Int32 FindEntry(const FontFamilyData& InData, const FontStyleKey& InStyle) const;

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        /** Nothing to acquire — a family holds paths, and this panel resolves none of them. */
        void Startup()     override {}

        /** Nothing the world's render depends on. */
        void OnPreRender() override {}

        void DrawContents() override;

        /** Release the sample's claim while the ResourceManager and the GL context are both alive (LC3). */
        void Shutdown()    override;

        PanelWindowStyle GetWindowStyle() const override { return { { 620.f, 460.f } }; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorContext& m_Context;

        /** Which plane of the matrix is on screen. Presentation, not document state. */
        EFontWidth m_PlaneWidth = EFontWidth::Normal;
        EFontSlant m_PlaneSlant = EFontSlant::Normal;

        /** Which entry the matrix highlights. -1 = none. Presentation, not document state. */
        Int32 m_Selected = -1;

        /** The style DrawResolve asks the family for. Presentation, not document state. */
        FontStyleKey m_Ask;

        // =============================================================================
        // The sample — what the selected face looks like, and the claim keeping it resident
        // =============================================================================
        OpaaxString m_SampleText = "Sphinx of black quartz,\njudge my vow. 0123";
        float       m_SampleSize = 48.f;

        /** The face the sample draws with. Re-claimed when the selection points somewhere else. */
        ResourceRef<FontFaceResource> m_SampleFace;
        OpaaxString                   m_SampleFacePath;

        // =============================================================================
        // The open edit gesture — the entry as it was when the first field went active
        // =============================================================================
        FontFamilyEntry m_GestureBefore;
        Uint32          m_GestureIndex   = 0;
        bool            m_bGestureOpen   = false;
        bool            m_bWasItemActive = false;
    };
}
