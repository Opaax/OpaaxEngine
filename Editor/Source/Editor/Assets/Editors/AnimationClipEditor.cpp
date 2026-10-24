#include "Editor/Assets/Editors/AnimationClipEditor.h"

#include <imgui.h>

#include "Animation/Assets/AnimationClip.h"
#include "Application/Services/IPaths.h"
#include "Core/Maths/Maths.h"
#include "Core/String/CString.h"
#include "Editor/Assets/AssetDocument.hpp"
#include "Editor/Assets/Editors/AnimationClipEditing.h"
#include "Editor/EditorContext.h"
#include "Editor/ImguiLibrary/ImguiLayout.h"
#include "Editor/ImguiLibrary/ImguiWidgets.h"
#include "Editor/Properties/PropertyDrawers.h"
#include "Editor/Resources/ResourceDragDrop.h"
#include "Editor/UI/IEditorWidgets.h"
#include "Renderer/Textures/SpriteSheet.h"
#include "Renderer/Textures/Texture2D.h"
#include "Resources/ResourceManager.h"

namespace Opaax::Editor
{
    AnimationClipEditor::~AnimationClipEditor() = default;

    void AnimationClipEditor::DrawContents(AssetDocument& InDocument)
    {
        AnimationClip* lClip = Cast<AnimationClip>(InDocument.GetAsset());
        if (lClip == nullptr)
        {
            return;
        }

        // An undo can shorten the list under the selection.
        if (m_Selected >= static_cast<Int32>(lClip->StepCount()))
        {
            m_Selected = -1;
        }

        DrawSummary(*lClip);
        DrawPreview(*lClip);
        ImGui::Separator();

        DrawSettings(*lClip);
        ImGui::Separator();

        DrawSelectedStep(*lClip);
        ImGui::Separator();

        DrawStepList(*lClip);
    }

    // =============================================================================
    // Preview
    // =============================================================================
    void AnimationClipEditor::DrawSummary(const AnimationClip& InClip)
    {
        const Uint32 lTicks   = InClip.TotalTicks();
        const float  lSeconds = (InClip.Fps > 0.f) ? static_cast<float>(lTicks) / InClip.Fps : 0.f;

        ImGui::TextDisabled("Steps: %u    Ticks: %u    Length: %.2fs", InClip.StepCount(), lTicks, lSeconds);
    }

    void AnimationClipEditor::DrawPreview(const AnimationClip& InClip)
    {
        if (m_bPreviewPlaying)
        {
            m_PreviewTime += ImGui::GetIO().DeltaTime;
        }

        const AnimationSample lSample = SampleClip(InClip, m_PreviewTime);

        if (ImGui::Button(m_bPreviewPlaying ? "Pause" : "Play"))
        {
            m_bPreviewPlaying = !m_bPreviewPlaying;
        }

        ImGui::SameLine();
        if (ImGui::Button("Restart"))
        {
            m_PreviewTime = 0.f;
        }

        ImGui::SameLine();
        ImGui::TextDisabled("step %u / %u", InClip.StepCount() > 0u ? lSample.Step + 1u : 0u, InClip.StepCount());

        // Scrubbing pauses the clock.
        const Uint32 lTicks  = InClip.TotalTicks();
        const float  lLength = (InClip.Fps > 0.f && lTicks > 0u) ? static_cast<float>(lTicks) / InClip.Fps : 0.f;

        if (lLength > 0.f)
        {
            float lScrub = (m_PreviewTime > lLength) ? Maths::FMod(m_PreviewTime, lLength) : m_PreviewTime;

            ImGui::SetNextItemWidth(-1.f);
            if (ImGui::SliderFloat("##scrub", &lScrub, 0.f, lLength, "%.2fs"))
            {
                m_PreviewTime     = lScrub;
                m_bPreviewPlaying = false;
            }
        }

        Vector2F          lPixelSize;
        const EditorImage lImage = ResolveStepImage(InClip, lSample.Step, lPixelSize);

        if (!lImage.IsValid())
        {
            // Said rather than drawn as an empty rectangle.
            ImGui::TextDisabled(InClip.StepCount() == 0u ? "No steps yet: press Add Step."
                                                         : "This step has no picture to show.");
            return;
        }

        ImguiWidgets::Image(lImage, ImguiLayout::AspectFit(static_cast<Uint32>(lPixelSize.x),
                                                           static_cast<Uint32>(lPixelSize.y), MAX_PREVIEW_SIZE));
    }

    EditorImage AnimationClipEditor::ResolveStepImage(const AnimationClip& InClip, const Uint32 InStepIndex,
                                                      Vector2F& OutPixelSize)
    {
        const AnimationStep* lStep = InClip.StepAt(InStepIndex);
        if (lStep == nullptr)
        {
            return EditorImage{};
        }

        // A texture clip: the step has its own image.
        if (InClip.Sheet.IsEmpty())
        {
            const Texture2D* lTexture = ClaimTexture(lStep->Texture.Path);
            if (lTexture == nullptr || lTexture->GetTexture() == nullptr)
            {
                return EditorImage{};
            }

            OutPixelSize = { static_cast<float>(lTexture->GetWidth()), static_cast<float>(lTexture->GetHeight()) };
            return m_Context.UIBackend.GetTextureImage(*lTexture->GetTexture());
        }

        // A sheet clip: the frame's rect, cropped from the sheet's image.
        const SpriteSheet* lSheet = ClaimSheet(InClip);
        const Int32        lIndex = (lSheet != nullptr) ? lSheet->FindFrameIndex(lStep->Frame) : -1;
        if (lIndex < 0)
        {
            return EditorImage{};
        }

        const SpriteFrame& lFrame = lSheet->Frames[static_cast<Uint32>(lIndex)];

        const Texture2D* lTexture = ClaimTexture(lSheet->Texture.Path);
        if (lTexture == nullptr || lTexture->GetTexture() == nullptr)
        {
            return EditorImage{};
        }

        OutPixelSize = lFrame.Size;
        return ClipEditing::CropToFrame(m_Context.UIBackend.GetTextureImage(*lTexture->GetTexture()), lFrame,
                                        lTexture->GetWidth(), lTexture->GetHeight());
    }

    // =============================================================================
    // Fields
    // =============================================================================
    void AnimationClipEditor::DrawSettings(AnimationClip& InClip)
    {
        // The steps have their own list below.
        TupleApply([this, &InClip](const auto&... lProperties)
                   {
                       ([&]
                        {
                            if (!CString::Equals(lProperties.Name, "Steps"))
                            {
                                DrawProperty(m_Context.Widgets, lProperties, InClip);
                            }
                        }(), ...);
                   },
                   AnimationClip::GetProperties());
    }

    void AnimationClipEditor::DrawSelectedStep(AnimationClip& InClip)
    {
        if (m_Selected < 0)
        {
            ImGui::TextDisabled("Select a step to edit it.");
            return;
        }

        AnimationStep& lStep  = InClip.Steps[static_cast<Uint32>(m_Selected)];
        const bool     bSheet = !InClip.Sheet.IsEmpty();

        ImGui::Text("Step %d", m_Selected);
        ImGui::PushID(m_Selected);

        // A sheet clip's step names a frame, picked among the sheet's names; a texture clip's step has a
        // texture. The field the clip does not read is not shown.
        TupleApply([this, &InClip, &lStep, bSheet](const auto&... lProperties)
                   {
                       ([&]
                        {
                            if (CString::Equals(lProperties.Name, bSheet ? "Texture" : "Frame"))
                            {
                                return;
                            }

                            if (bSheet && CString::Equals(lProperties.Name, "Frame"))
                            {
                                DrawFramePicker(InClip, lStep);
                                DrawPropertyNote(m_Context.Widgets, lProperties.Meta);
                                return;
                            }

                            DrawProperty(m_Context.Widgets, lProperties, lStep);
                        }(), ...);
                   },
                   AnimationStep::GetProperties());

        ImGui::PopID();
    }

    void AnimationClipEditor::DrawFramePicker(const AnimationClip& InClip, AnimationStep& InOutStep)
    {
        const SpriteSheet* lSheet = ClaimSheet(InClip);
        if (lSheet == nullptr)
        {
            ImGui::TextDisabled("The clip's sheet did not load: no frame to pick.");
            return;
        }

        IEditorWidgets& lWidgets = m_Context.Widgets;

        if (!lWidgets.BeginCombo("Frame", InOutStep.Frame.IsValid() ? InOutStep.Frame.CStr() : "(none)"))
        {
            return;
        }

        Uint32 lNamed = 0;

        for (Uint32 lIndex = 0; lIndex < lSheet->FrameCount(); ++lIndex)
        {
            const SpriteFrame& lFrame = lSheet->Frames[lIndex];

            // A step names its frame: an unnamed one cannot be picked.
            if (!lFrame.Name.IsValid())
            {
                continue;
            }

            ++lNamed;

            ImGui::PushID(static_cast<int>(lIndex));

            const bool bSelected = lFrame.Name == InOutStep.Frame;
            if (lWidgets.Selectable(lFrame.Name.CStr(), bSelected))
            {
                InOutStep.Frame = lFrame.Name;
            }

            if (bSelected)
            {
                lWidgets.SetDefaultFocus();
            }

            ImGui::PopID();
        }

        if (lNamed == 0)
        {
            ImGui::TextDisabled("No named frames in this sheet: name them in its editor.");
        }

        lWidgets.EndCombo();
    }

    // =============================================================================
    // Steps
    // =============================================================================
    void AnimationClipEditor::DrawStepList(AnimationClip& InClip)
    {
        TDynArray<AnimationStep>& lSteps = InClip.Steps;

        if (ImGui::Button("Add Step"))
        {
            ClipEditing::AddStep(lSteps);
            m_Selected = static_cast<Int32>(lSteps.size()) - 1;
        }
        ImGui::SetItemTooltip("Adds a copy of the last step.");

        ImGui::SameLine();
        ImGui::BeginDisabled(m_Selected < 0);

        if (ImGui::Button("Remove") && m_Selected >= 0)
        {
            lSteps.erase(lSteps.begin() + m_Selected);
            m_Selected = -1;
        }

        ImGui::SameLine();
        if (ImGui::Button("Up") && m_Selected > 0)
        {
            const Uint32 lIndex = static_cast<Uint32>(m_Selected);
            Swap(lSteps[lIndex], lSteps[lIndex - 1]);
            --m_Selected;
        }

        ImGui::SameLine();
        if (ImGui::Button("Down") && m_Selected >= 0 && m_Selected + 1 < static_cast<Int32>(lSteps.size()))
        {
            const Uint32 lIndex = static_cast<Uint32>(m_Selected);
            Swap(lSteps[lIndex], lSteps[lIndex + 1]);
            ++m_Selected;
        }

        ImGui::EndDisabled();

        // A texture clip is built by dropping textures in, a step each; a sheet clip names frames instead.
        const bool bSheet = !InClip.Sheet.IsEmpty();
        if (!bSheet)
        {
            ImGui::Button("Drop a texture here to add a step", ImVec2(-1.f, 0.f));

            if (OpaaxString lDropped; AcceptAssetDragPayload<Texture2D>(lDropped))
            {
                if (ClipEditing::AddTextureStep(lSteps, lDropped))
                {
                    m_Selected = static_cast<Int32>(lSteps.size()) - 1;
                }
            }
        }

        if (lSteps.empty())
        {
            return;
        }

        constexpr ImGuiTableFlags lFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV;

        if (!ImGui::BeginTable("##steps", 3, lFlags))
        {
            return;
        }

        ImGui::TableSetupColumn("#",                          ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("000").x);
        ImGui::TableSetupColumn(bSheet ? "Frame" : "Texture", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Hold",                       ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("Hold").x);
        ImGui::TableHeadersRow();

        for (Uint32 lIndex = 0; lIndex < lSteps.size(); ++lIndex)
        {
            const AnimationStep& lStep = lSteps[lIndex];

            const char* lPicture = bSheet ? (lStep.Frame.IsValid() ? lStep.Frame.CStr() : "(no frame)")
                                          : (lStep.Texture.IsEmpty() ? "(no texture)" : lStep.Texture.Path.CStr());

            ImGui::TableNextRow();
            ImGui::PushID(static_cast<int>(lIndex));

            char lNumber[16];
            CString::Format(lNumber, "{}", lIndex);

            ImGui::TableSetColumnIndex(0);
            if (ImGui::Selectable(lNumber, m_Selected == static_cast<Int32>(lIndex), ImGuiSelectableFlags_SpanAllColumns))
            {
                m_Selected = static_cast<Int32>(lIndex);
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(lPicture);

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("x%u", lStep.EffectiveHold());

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    // =============================================================================
    // Claims
    // =============================================================================
    const SpriteSheet* AnimationClipEditor::ClaimSheet(const AnimationClip& InClip)
    {
        if (InClip.Sheet.IsEmpty())
        {
            m_Sheet            = {};
            m_ClaimedSheetPath = OpaaxString();
            return nullptr;
        }

        // Compared with the path asked for: the handle keeps the converted one when it names an old file.
        if (InClip.Sheet.Path != m_ClaimedSheetPath)
        {
            m_Sheet            = LoadAsset(m_Context.Resources, m_Context.Paths, InClip.Sheet);
            m_ClaimedSheetPath = InClip.Sheet.Path;

            if (m_Sheet.Get() == nullptr)
            {
                OPAAX_LOG(LogAnimationClipEditor, Warn, "Clip sheet '{}' did not load", InClip.Sheet.Path.CStr());
            }
        }

        return m_Sheet.Get();
    }

    const Texture2D* AnimationClipEditor::ClaimTexture(const OpaaxString& InAssetPath)
    {
        if (InAssetPath.IsEmpty())
        {
            return nullptr;
        }

        const Uint32 lKey = OpaaxStringID(InAssetPath).GetId();

        auto lIt = m_TextureClaims.find(lKey);
        if (lIt == m_TextureClaims.end())
        {
            lIt = m_TextureClaims.emplace(lKey, LoadAsset<Texture2D>(m_Context.Resources, m_Context.Paths,
                                                                     InAssetPath)).first;

            if (lIt->second.Get() == nullptr)
            {
                OPAAX_LOG(LogAnimationClipEditor, Warn, "Clip image '{}' did not load", InAssetPath.CStr());
            }
        }

        return lIt->second.Get();
    }
}
