#include "Editor/Assets/Editors/FontFamilyEditor.h"

#include <imgui.h>

#include "Application/Services/IPaths.h"
#include "Editor/Assets/AssetDocument.hpp"
#include "Editor/Assets/Editors/FontFamilyEditing.h"
#include "Editor/EditorContext.h"
#include "Editor/Properties/PropertyDrawers.h"
#include "Editor/UI/IEditorUIBackend.h"   // the atlas as an image
#include "Renderer/Text/FontFaceResource.h"
#include "Renderer/Text/FontFamily.h"
#include "Renderer/Text/Text2D.h"         // Layout: the same as the viewport
#include "Resources/ResourceManager.h"

namespace Opaax::Editor
{
    FontFamilyEditor::~FontFamilyEditor() = default;

    void FontFamilyEditor::DrawContents(AssetDocument& InDocument)
    {
        FontFamily* lFamily = Cast<FontFamily>(InDocument.GetAsset());
        if (lFamily == nullptr)
        {
            return;
        }

        // An undo can shorten the list under the selection.
        if (m_Selected >= static_cast<Int32>(lFamily->EntryCount()))
        {
            m_Selected = -1;
        }

        ImGui::TextDisabled("Faces: %u", lFamily->EntryCount());

        DrawPlaneSelectors();
        DrawMatrix(*lFamily);
        ImGui::Separator();

        DrawSelectedEntry(*lFamily);
        ImGui::Separator();

        DrawSample(*lFamily);
        DrawResolve(*lFamily);
    }

    // =============================================================================
    // The matrix
    // =============================================================================
    void FontFamilyEditor::DrawPlaneSelectors()
    {
        // The matrix is script x weight; width and slant are picked above it (81 cells, not 648).
        ImGui::SetNextItemWidth(140.f);
        if (ImGui::BeginCombo("Width", ToString(m_PlaneWidth)))
        {
            for (const EFontWidth lCandidate : TEnumValues<EFontWidth>::Values)
            {
                if (ImGui::Selectable(ToString(lCandidate), lCandidate == m_PlaneWidth))
                {
                    m_PlaneWidth = lCandidate;
                }
            }

            ImGui::EndCombo();
        }

        ImGui::SameLine();

        ImGui::SetNextItemWidth(140.f);
        if (ImGui::BeginCombo("Slant", ToString(m_PlaneSlant)))
        {
            for (const EFontSlant lCandidate : TEnumValues<EFontSlant>::Values)
            {
                if (ImGui::Selectable(ToString(lCandidate), lCandidate == m_PlaneSlant))
                {
                    m_PlaneSlant = lCandidate;
                }
            }

            ImGui::EndCombo();
        }
    }

    void FontFamilyEditor::DrawMatrix(FontFamily& InFamily)
    {
        constexpr Uint32 SUBSET_COUNT = static_cast<Uint32>(ArraySize(TEnumValues<EFontSubset>::Values));
        constexpr Uint32 WEIGHT_COUNT = static_cast<Uint32>(ArraySize(TEnumValues<EFontWeight>::Values));

        // Every weight and the header row, so the sections below stay in the window. A row is a line of
        // text and its cell padding.
        const float  lRowHeight = ImGui::GetTextLineHeight() + ImGui::GetStyle().CellPadding.y * 2.f;
        const ImVec2 lSize(0.f, lRowHeight * static_cast<float>(WEIGHT_COUNT + 1u) + ImGui::GetStyle().ScrollbarSize);

        // +1 for the weight labels.
        if (!ImGui::BeginTable("FamilyMatrix", static_cast<int>(SUBSET_COUNT) + 1,
                               ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit
                             | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY, lSize))
        {
            return;
        }

        ImGui::TableSetupScrollFreeze(1, 1);   // the weight column and the script row stay in view
        ImGui::TableSetupColumn("Weight");

        for (const EFontSubset lSubset : TEnumValues<EFontSubset>::Values)
        {
            ImGui::TableSetupColumn(ToString(lSubset));
        }

        ImGui::TableHeadersRow();

        for (const EFontWeight lWeight : TEnumValues<EFontWeight>::Values)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(ToString(lWeight));

            for (const EFontSubset lSubset : TEnumValues<EFontSubset>::Values)
            {
                ImGui::TableNextColumn();

                const FontStyleKey lStyle{ lSubset, lWeight, m_PlaneWidth, m_PlaneSlant };
                const Int32        lIndex = FamilyEditing::FindEntry(InFamily.Entries, lStyle);

                // An id per cell, or two cells of a row would share one.
                ImGui::PushID(static_cast<int>(lWeight));
                ImGui::PushID(static_cast<int>(lSubset));

                if (lIndex < 0)
                {
                    // A missing style: the button adds it (hence no separate Add button).
                    if (ImGui::SmallButton("+"))
                    {
                        m_Selected = FamilyEditing::AddEntry(InFamily.Entries, lStyle);
                    }
                }
                else
                {
                    // An entry without a file yet looks different from one that resolves.
                    const bool bEmpty = InFamily.Entries[static_cast<Uint32>(lIndex)].Face.IsEmpty();

                    if (ImGui::Selectable(bEmpty ? "?" : "*", m_Selected == lIndex))
                    {
                        m_Selected = lIndex;
                    }
                }

                ImGui::PopID();
                ImGui::PopID();
            }
        }

        ImGui::EndTable();
    }

    // =============================================================================
    // The selected face
    // =============================================================================
    void FontFamilyEditor::DrawSelectedEntry(FontFamily& InFamily)
    {
        if (m_Selected < 0)
        {
            ImGui::TextDisabled("Pick a cell above: '*' has a face, '?' has none yet, '+' adds one.");
            return;
        }

        const Uint32       lIndex       = static_cast<Uint32>(m_Selected);
        FontFamilyEntry&   lEntry       = InFamily.Entries[lIndex];
        const FontStyleKey lStyleBefore = lEntry.Style;

        ImGui::PushID(static_cast<int>(lIndex));
        DrawProperties(m_Context.Widgets, lEntry);
        ImGui::PopID();

        // The dropdowns already wrote the style: one another face has is put back.
        const FontStyleKey lWanted = lEntry.Style;
        if (FamilyEditing::RevertTakenStyle(InFamily.Entries, lIndex, lStyleBefore))
        {
            OPAAX_LOG(LogFontFamilyEditor, Warn, "{}/{}/{}/{} is already in this family — the change was undone",
                      ToString(lWanted.Subset), ToString(lWanted.Weight), ToString(lWanted.Width), ToString(lWanted.Slant));
        }
        else if (lEntry.Style != lStyleBefore)
        {
            // The matrix follows the face, so its cell stays in view.
            m_PlaneWidth = lEntry.Style.Width;
            m_PlaneSlant = lEntry.Style.Slant;
        }

        if (ImGui::SmallButton("Remove Face"))
        {
            InFamily.Entries.erase(InFamily.Entries.begin() + m_Selected);
            m_Selected = -1;
        }
    }

    void FontFamilyEditor::ClaimSampleFace(const OpaaxString& InPath)
    {
        if (InPath == m_SampleFacePath)
        {
            return;   // already held (runs every frame)
        }

        m_SampleFacePath = InPath;
        m_SampleFace     = ResourceRef<FontFaceResource>();   // released before the next is claimed

        if (InPath.IsEmpty())
        {
            return;
        }

        const OpaaxString lAbsolute = m_Context.Paths.AssetToAbsolute(InPath);
        m_SampleFace = m_Context.Resources.Load<FontFaceResource>(lAbsolute.CStr());
    }

    void FontFamilyEditor::DrawSample(const FontFamily& InFamily)
    {
        if (!ImGui::TreeNodeEx("Sample", ImGuiTreeNodeFlags_DefaultOpen))
        {
            return;
        }

        const FontFamilyEntry* lEntry = (m_Selected >= 0) ? &InFamily.Entries[static_cast<Uint32>(m_Selected)] : nullptr;

        ClaimSampleFace(lEntry != nullptr ? lEntry->Face.Path : OpaaxString());

        DrawField(m_Context.Widgets, "Text", m_SampleText, PropertyMeta{ .Flags = EPropertyFlags::Multiline });

        ImGui::SetNextItemWidth(160.f);
        ImGui::DragFloat("Size", &m_SampleSize, 0.5f, 6.f, 200.f, "%.0f px");

        const FontFaceResource* lFace = m_SampleFace.Get();

        if (lFace == nullptr)
        {
            ImGui::TextDisabled(lEntry == nullptr ? "Pick a cell to see its face." : "That cut names no file yet.");
            ImGui::TreePop();
            return;
        }

        if (lFace->GetAtlas() == nullptr)
        {
            // The bake ran on load; the atlas uploads on the next pump.
            ImGui::TextDisabled("Uploading the atlas...");
            ImGui::TreePop();
            return;
        }

        // The viewport's layout, drawn by ImGui. World Y is up and ImGui's is down: Y is negated.
        const EditorImage lImage  = m_Context.UIBackend.GetTextureImage(*lFace->GetAtlas());
        const ImVec2      lOrigin = ImGui::GetCursorScreenPos();
        ImDrawList* const lDraw   = ImGui::GetWindowDrawList();
        const ImU32       lColour = ImGui::GetColorU32(ImGuiCol_Text);

        TextDrawParams lParams;
        lParams.Size = m_SampleSize;

        const FontFaceView lView{ &lFace->Face, lFace->GetAtlas() };

        const Vector2F lExtent = Text2D::Layout(
            m_SampleText.CStr(), { 0.f, 0.f }, lView, lParams,
            [lDraw, lOrigin, lColour, &lImage](const TextQuad& InQuad)
            {
                const ImVec2 lMin(lOrigin.x + InQuad.Centre.x - InQuad.Size.x * 0.5f,
                                  lOrigin.y - InQuad.Centre.y - InQuad.Size.y * 0.5f);
                const ImVec2 lMax(lMin.x + InQuad.Size.x, lMin.y + InQuad.Size.y);

                if (InQuad.bTofu)
                {
                    lDraw->AddRect(lMin, lMax, lColour);
                    return;
                }

                // UVMin is the quad's bottom edge; ImGui's uv0 is the top-left.
                lDraw->AddImage(static_cast<ImTextureID>(lImage.Handle), lMin, lMax,
                                ImVec2(InQuad.UVMin.x, InQuad.UVMax.y),
                                ImVec2(InQuad.UVMax.x, InQuad.UVMin.y), lColour);
            });

        // Drawing into the draw list does not move the cursor: the box is reserved afterwards.
        ImGui::Dummy(ImVec2(lExtent.x, lExtent.y));

        ImGui::TextDisabled("%s", m_SampleFacePath.CStr());

        ImGui::TreePop();
    }

    // =============================================================================
    // Resolve
    // =============================================================================
    void FontFamilyEditor::DrawResolve(const FontFamily& InFamily)
    {
        if (!ImGui::TreeNodeEx("Resolve", ImGuiTreeNodeFlags_DefaultOpen))
        {
            return;
        }

        ImGui::TextDisabled("What a TextComponent asking for this style would get.");

        ImGui::PushID("ask");
        DrawProperties(m_Context.Widgets, m_Ask);
        ImGui::PopID();

        const FontFamilyEntry* lFound = InFamily.Find(m_Ask);

        if (lFound == nullptr)
        {
            // No face for this script (no fallback): said in full.
            ImGui::TextDisabled("No %s face in this family.", ToString(m_Ask.Subset));
            ImGui::TextDisabled("Text asking for it draws a row of boxes.");
        }
        else if (lFound->Style == m_Ask)
        {
            ImGui::TextDisabled("Exact.");
            ImGui::TextDisabled("%s", lFound->Face.IsEmpty() ? "(this cut names no file yet)" : lFound->Face.Path.CStr());
        }
        else
        {
            // The script always matches, so one of the other three differs: both styles are named.
            ImGui::TextDisabled("No %s / %s / %s: falls back to %s / %s / %s",
                                ToString(m_Ask.Width), ToString(m_Ask.Slant), ToString(m_Ask.Weight),
                                ToString(lFound->Style.Width), ToString(lFound->Style.Slant),
                                ToString(lFound->Style.Weight));

            ImGui::TextDisabled("%s", lFound->Face.IsEmpty() ? "(that cut names no file yet)" : lFound->Face.Path.CStr());
        }

        if (lFound != nullptr && ImGui::SmallButton("Select it"))
        {
            m_Selected   = FamilyEditing::FindEntry(InFamily.Entries, lFound->Style);
            m_PlaneWidth = lFound->Style.Width;
            m_PlaneSlant = lFound->Style.Slant;
        }

        ImGui::TreePop();
    }
}
