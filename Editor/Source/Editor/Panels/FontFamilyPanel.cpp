#include "Editor/Panels/FontFamilyPanel.h"

#include "Editor/EditorContext.h"
#include "Editor/EditorFontFamilyDocument.h"
#include "Editor/Commands/EditorCommandRegistry.h"
#include "Editor/Commands/EditorNativeCommandsTags.hpp"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/Operation/FontFamilyOperations.h"
#include "Editor/Properties/PropertyDrawers.h"   // the specializations DrawProperties folds over
#include "Editor/UI/IEditorUIBackend.h"          // the atlas as an ImGui image

#include "Application/Services/IPaths.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Engine/Subsystems/Resources/Types/Font/FontFaceResource.h"
#include "Renderer/Text/Text2D.h"                // Layout — the SAME walk the viewport uses

#include <imgui.h>

using namespace Opaax;

namespace Opaax::Editor
{
    FontFamilyPanel::FontFamilyPanel(EditorContext& InContext)
        : m_Context(InContext)
    {
    }

    FontFamilyPanel::~FontFamilyPanel() = default;

    void FontFamilyPanel::DrawContents()
    {
        if (!m_Context.FamilyDocument.IsOpen())
        {
            ImGui::TextDisabled("No font family open.");
            ImGui::TextDisabled("Double-click a .opaaxfont in the Resource Browser.");

            m_Selected = -1;
            return;
        }

        FontFamilyData& lData = m_Context.FamilyDocument.GetMutableData();

        DrawHeader(lData);
        ImGui::Separator();

        DrawSelectedEntry(lData);
        ImGui::Separator();

        DrawSample(lData);
        ImGui::Separator();

        DrawPlaneSelectors();
        DrawMatrix(lData);

        ImGui::Separator();
        DrawResolve(lData);
    }

    void FontFamilyPanel::Shutdown()
    {
        m_SampleFace     = ResourceRef<FontFaceResource>();
        m_SampleFacePath = OpaaxString();
    }

    void FontFamilyPanel::ClaimSampleFace(const OpaaxString& InPath)
    {
        if (InPath == m_SampleFacePath)
        {
            return;   // already held — this runs every frame
        }

        m_SampleFacePath = InPath;
        m_SampleFace     = ResourceRef<FontFaceResource>();   // release before claiming the next

        if (InPath.IsEmpty())
        {
            return;
        }

        const OpaaxString lAbsolute = m_Context.Paths.AssetToAbsolute(InPath);
        m_SampleFace = m_Context.Resources.Load<FontFaceResource>(lAbsolute.CStr());
    }

    void FontFamilyPanel::DrawSample(const FontFamilyData& InData)
    {
        if (!ImGui::TreeNodeEx("Sample", ImGuiTreeNodeFlags_DefaultOpen)) { return; }

        const FontFamilyEntry* lEntry = (m_Selected >= 0 && static_cast<Uint32>(m_Selected) < InData.EntryCount())
                                            ? &InData.Entries[static_cast<Uint32>(m_Selected)]
                                            : nullptr;

        ClaimSampleFace(lEntry != nullptr ? lEntry->Face.Path : OpaaxString());

        DrawField(m_Context.Widgets, "Text", m_SampleText,
                  PropertyMeta{ .Flags = EPropertyFlags::Multiline });

        ImGui::SetNextItemWidth(160.f);
        ImGui::DragFloat("Size", &m_SampleSize, 0.5f, 6.f, 200.f, "%.0f px");

        const FontFaceResource* lFace = m_SampleFace.Get();

        if (lFace == nullptr)
        {
            ImGui::TextDisabled(lEntry == nullptr ? "Pick a cell to see its face."
                                                  : "That cut names no file yet.");
            ImGui::TreePop();
            return;
        }

        if (lFace->GetAtlas() == nullptr)
        {
            // A frame or two: the bake ran on Load, the UPLOAD runs at the next pump (TX4).
            ImGui::TextDisabled("Uploading the atlas...");
            ImGui::TreePop();
            return;
        }

        // ONE walk, an ImGui sink. The world's Y goes up and ImGui's goes down, so the origin is the
        // top-left of the box and every quad's Y is negated into it.
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

                // THE V SWAP, per glyph. The quad carries the world's convention — UVMin is its
                // BOTTOM edge — and ImGui's uv0 goes with the TOP-left corner (**TX9**).
                lDraw->AddImage(static_cast<ImTextureID>(lImage.Handle), lMin, lMax,
                                ImVec2(InQuad.UVMin.x, InQuad.UVMax.y),
                                ImVec2(InQuad.UVMax.x, InQuad.UVMin.y), lColour);
            });

        // The layout DREW into the draw list, which does not advance the cursor — so the box has to
        // be reserved afterwards, or everything below would draw on top of the sample.
        ImGui::Dummy(ImVec2(lExtent.x, lExtent.y));

        ImGui::TextDisabled("%s", m_SampleFacePath.CStr());

        ImGui::TreePop();
    }

    void FontFamilyPanel::DrawResolve(const FontFamilyData& InData)
    {
        if (!ImGui::TreeNodeEx("Resolve", ImGuiTreeNodeFlags_DefaultOpen)) { return; }

        ImGui::TextDisabled("What a TextComponent asking for this style would get.");

        ImGui::PushID("ask");
        DrawProperties(m_Context.Widgets, m_Ask);
        ImGui::PopID();

        const FontFamilyEntry* lFound = InData.Find(m_Ask);

        if (lFound == nullptr)
        {
            // The one axis with no fallback, and the loudest outcome — worth saying in full rather
            // than leaving it as an empty result.
            ImGui::TextDisabled("No %s face in this family.", ToString(m_Ask.Subset));
            ImGui::TextDisabled("Text asking for it draws a row of boxes.");
        }
        else if (lFound->Style == m_Ask)
        {
            ImGui::TextDisabled("Exact.");
            ImGui::TextDisabled("%s", lFound->Face.IsEmpty() ? "(this cut names no file yet)"
                                                             : lFound->Face.Path.CStr());
        }
        else
        {
            // The subset always matches — Find refuses to cross it — so what differed is one of the
            // other three, and naming both cuts says which.
            ImGui::TextDisabled("No %s / %s / %s — falls back to %s / %s / %s",
                                ToString(m_Ask.Width),      ToString(m_Ask.Slant),      ToString(m_Ask.Weight),
                                ToString(lFound->Style.Width), ToString(lFound->Style.Slant),
                                ToString(lFound->Style.Weight));

            ImGui::TextDisabled("%s", lFound->Face.IsEmpty() ? "(that cut names no file yet)"
                                                             : lFound->Face.Path.CStr());
        }

        if (lFound != nullptr && ImGui::SmallButton("Select it"))
        {
            m_Selected   = FindEntry(InData, lFound->Style);
            m_PlaneWidth = lFound->Style.Width;
            m_PlaneSlant = lFound->Style.Slant;
        }

        ImGui::TreePop();
    }

    void FontFamilyPanel::DrawHeader(const FontFamilyData& InData)
    {
        const OpaaxString lName  = m_Context.FamilyDocument.FileName();
        const bool        bDirty = m_Context.FamilyDocument.IsDirty();

        ImGui::Text("%s%s", lName.CStr(), bDirty ? " *" : "");

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 46.f);

        ImGui::BeginDisabled(!bDirty);
        if (ImGui::SmallButton("Save"))
        {
            m_Context.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_SAVE_FAMILY, m_Context);
        }
        ImGui::EndDisabled();

        ImGui::TextDisabled("Faces : %u", InData.EntryCount());
    }

    void FontFamilyPanel::DrawPlaneSelectors()
    {
        // The matrix is subset x weight, so width and slant have to be chosen OUTSIDE it. Two
        // combos rather than a 4x larger table: 81 cells reads, 324 does not.
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

    Int32 FontFamilyPanel::FindEntry(const FontFamilyData& InData, const FontStyleKey& InStyle) const
    {
        for (Uint32 lIndex = 0; lIndex < InData.EntryCount(); ++lIndex)
        {
            if (InData.Entries[lIndex].Style == InStyle) { return static_cast<Int32>(lIndex); }
        }

        return -1;
    }

    void FontFamilyPanel::DrawMatrix(const FontFamilyData& InData)
    {
        constexpr Uint32 SUBSET_COUNT = static_cast<Uint32>(std::size(TEnumValues<EFontSubset>::Values));

        // +1 for the weight labels down the left.
        if (!ImGui::BeginTable("FamilyMatrix", static_cast<int>(SUBSET_COUNT) + 1,
                               ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit
                             | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY))
        {
            return;
        }

        ImGui::TableSetupScrollFreeze(1, 1);   // the weight column and the subset row stay put
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
                const Int32        lIndex = FindEntry(InData, lStyle);

                // PushID per cell, not per row: two cells in one row would otherwise share an id and
                // ImGui would route both clicks to the first (I16).
                ImGui::PushID(static_cast<int>(lWeight));
                ImGui::PushID(static_cast<int>(lSubset));

                if (lIndex < 0)
                {
                    // A face this family does not have. The button ADDS it, already carrying its
                    // style — which is why there is no separate Add button anywhere on this panel.
                    if (ImGui::SmallButton("+") && FamilyOps::AddEntry(m_Context, lStyle))
                    {
                        m_Selected = static_cast<Int32>(InData.EntryCount()) - 1;
                    }
                }
                else
                {
                    const bool bSelected = (m_Selected == lIndex);
                    const bool bEmpty    = InData.Entries[static_cast<Uint32>(lIndex)].Face.IsEmpty();

                    // An entry with no file yet is a real state — just added, or cleared — and it has
                    // to look different from one that resolves, or a half-built family reads as done.
                    if (ImGui::Selectable(bEmpty ? "?" : "*", bSelected))
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

    void FontFamilyPanel::DrawSelectedEntry(FontFamilyData& InData)
    {
        if (m_Selected < 0 || static_cast<Uint32>(m_Selected) >= InData.EntryCount())
        {
            ImGui::TextDisabled("Pick a cell below. '*' has a face, '?' has none, '+' adds one.");
            return;
        }

        const Uint32 lIndex = static_cast<Uint32>(m_Selected);

        ImGui::PushID(static_cast<int>(lIndex));
        DrawProperties(m_Context.Widgets, InData.Entries[lIndex]);
        ImGui::PopID();

        if (ImGui::SmallButton("Remove Face") && FamilyOps::RemoveEntry(m_Context, lIndex))
        {
            m_Selected       = -1;
            m_bGestureOpen   = false;
            m_bWasItemActive = false;
            return;   // the list just changed under everything below
        }

        // The Inspector's bracket: a TPropertyDrawer writes straight through a reference and cannot
        // report that it did, so the edges of "any item is active" open and close one step. The
        // CLOSE goes through FamilyOps, which is what judges the style against the rest of the family.
        const bool lItemActive = ImGui::IsAnyItemActive();

        if (lItemActive && !m_bWasItemActive)
        {
            m_GestureBefore = InData.Entries[lIndex];
            m_GestureIndex  = lIndex;
            m_bGestureOpen  = true;
        }
        else if (!lItemActive && m_bWasItemActive && m_bGestureOpen)
        {
            FamilyOps::CommitEntryEdit(m_Context, m_GestureIndex, m_GestureBefore);

            m_GestureBefore = FontFamilyEntry{};
            m_bGestureOpen  = false;
        }

        m_bWasItemActive = lItemActive;
    }
}
