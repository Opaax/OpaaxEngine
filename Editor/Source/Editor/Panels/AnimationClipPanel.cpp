#include "Editor/Panels/AnimationClipPanel.h"

#include <cmath>    // fmod
#include <cstdio>   // snprintf

#include "Editor/Resources/Types/Animation/EditorAnimationClipDocument.h"
#include "Editor/EditorContext.h"
#include "Editor/ImguiLibrary/ImguiLayout.h"
#include "Editor/ImguiLibrary/ImguiWidgets.h"
#include "Editor/Commands/EditorCommandRegistry.h"
#include "Editor/Commands/EditorNativeCommandsTags.hpp"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/Operation/ClipOperations.h"
#include "Editor/Properties/PropertyDrawers.h"
#include "Editor/Resources/ResourceDragDrop.h"   // the step list accepts dropped textures
#include "Editor/Undo/EditorUndo.h"
#include "Editor/UI/IEditorUIBackend.h"

#include "Engine/Subsystems/Resources/ResourceTypeID.hpp"

#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Engine/Subsystems/Resources/Types/Animation/AnimationClipData.h"
#include "Engine/Subsystems/Resources/Types/SpriteSheet/SpriteSheetData.h"
#include "Engine/Subsystems/Resources/Types/SpriteSheet/SpriteSheetResource.h"
#include "Engine/Subsystems/Resources/Types/Texture/TextureResource.h"
#include "Application/Services/IPaths.h"

#include <imgui.h>

using namespace Opaax;

namespace Opaax::Editor
{
    namespace
    {
        /** Where t of the way from InFrom to InTo lands. */
        float Lerp(const float InFrom, const float InTo, const float InT) noexcept
        {
            return InFrom + (InTo - InFrom) * InT;
        }

        /**
         * InFull cropped to InFrame's pixel rect, interpolated inside the backend's UVs (so the flip
         * always matches the backend).
         */
        EditorImage CropTo(const EditorImage& InFull, const SpriteFrame& InFrame,
                           const Uint32 InTexWidth, const Uint32 InTexHeight)
        {
            const float lWidth  = static_cast<float>(InTexWidth);
            const float lHeight = static_cast<float>(InTexHeight);

            if (lWidth <= 0.f || lHeight <= 0.f || InFrame.Size.x <= 0.f || InFrame.Size.y <= 0.f)
            {
                return InFull;   // bad frame: show everything
            }

            EditorImage lCropped = InFull;

            lCropped.UV0.x = Lerp(InFull.UV0.x, InFull.UV1.x, InFrame.Offset.x / lWidth);
            lCropped.UV1.x = Lerp(InFull.UV0.x, InFull.UV1.x, (InFrame.Offset.x + InFrame.Size.x) / lWidth);
            lCropped.UV0.y = Lerp(InFull.UV0.y, InFull.UV1.y, InFrame.Offset.y / lHeight);
            lCropped.UV1.y = Lerp(InFull.UV0.y, InFull.UV1.y, (InFrame.Offset.y + InFrame.Size.y) / lHeight);

            return lCropped;
        }
    }

    AnimationClipPanel::AnimationClipPanel(EditorContext& InContext)
        : m_Context(InContext)
    {
    }

    AnimationClipPanel::~AnimationClipPanel() = default;

    void AnimationClipPanel::Shutdown()
    {
        m_SheetClaim         = {};
        m_TextureClaim       = {};
        m_ClaimedSheetPath   = OpaaxString();
        m_ClaimedTexturePath = OpaaxString();
    }

    // =============================================================================
    // Draw
    // =============================================================================
    void AnimationClipPanel::DrawContents()
    {
        if (!m_Context.ClipDocument.IsOpen())
        {
            ImGui::TextDisabled("No animation clip open.");
            ImGui::TextDisabled("Double-click a .opaaxclip in the Resource Browser.");

            // Nothing open: nothing to keep loaded.
            Shutdown();
            m_Selected    = -1;
            m_PreviewTime = 0.f;
            return;
        }

        AnimationClipData& lData = m_Context.ClipDocument.GetMutableData();

        DrawHeader(lData);
        ImGui::Separator();

        DrawPreview(lData);
        ImGui::Separator();

        DrawSettings(lData);
        ImGui::Separator();

        DrawSelectedStep(lData);
        ImGui::Separator();

        DrawStepList(lData);
    }

    void AnimationClipPanel::DrawHeader(const AnimationClipData& InData)
    {
        const OpaaxString lName  = m_Context.ClipDocument.FileName();
        const bool        bDirty = m_Context.ClipDocument.IsDirty();

        ImGui::Text("%s%s", lName.CStr(), bDirty ? " *" : "");

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 46.f);

        // The button and Ctrl+S dispatch the same command tag.
        ImGui::BeginDisabled(!bDirty);
        if (ImGui::SmallButton("Save"))
        {
            m_Context.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_SAVE_CLIP, m_Context);
        }
        ImGui::EndDisabled();

        const Uint32 lTicks = InData.TotalTicks();
        const float  lSeconds = (InData.Fps > 0.f) ? static_cast<float>(lTicks) / InData.Fps : 0.f;

        ImGui::TextDisabled("Sheet : %s", InData.Sheet.IsEmpty() ? "(none — steps carry textures)"
                                                                 : InData.Sheet.Path.CStr());
        ImGui::TextDisabled("Steps : %u    Ticks : %u    Length : %.2fs",
                            InData.StepCount(), lTicks, lSeconds);
    }

    void AnimationClipPanel::DrawSettings(AnimationClipData& InData)
    {
        if (!ImGui::TreeNodeEx("Clip", ImGuiTreeNodeFlags_DefaultOpen)) { return; }

        // The clip's own fields, from its property list.
        ImGui::PushID("ClipSettings");
        DrawProperties(m_Context.Widgets, InData);
        ImGui::PopID();

        // Like the Inspector: a drawer writes through a reference without reporting it, so an undo step
        // opens when an item becomes active and closes when none is.
        const bool lItemActive = ImGui::IsAnyItemActive();

        if (lItemActive && !m_bWasSettingsItemActive)
        {
            m_SettingsGesture.Begin(m_Context);
            m_bSettingsGestureOpen = true;
        }
        else if (!lItemActive && m_bWasSettingsItemActive && m_bSettingsGestureOpen)
        {
            if (m_SettingsGesture.End(m_Context)) { m_Context.Undo.Record(Move(m_SettingsGesture)); }

            m_SettingsGesture      = ClipSettingsEdit{};
            m_bSettingsGestureOpen = false;
        }

        m_bWasSettingsItemActive = lItemActive;

        ImGui::TreePop();
    }

    void AnimationClipPanel::DrawPreview(const AnimationClipData& InData)
    {
        // The panel's own clock: Edit worlds have no SpriteAnimationSubsystem, and the preview shows the
        // document's copy.
        if (m_bPreviewPlaying)
        {
            m_PreviewTime += ImGui::GetIO().DeltaTime;
        }

        const AnimationSample lSample = SampleClip(InData, m_PreviewTime);

        if (ImGui::Button(m_bPreviewPlaying ? "Pause" : "Play")) { m_bPreviewPlaying = !m_bPreviewPlaying; }

        ImGui::SameLine();
        if (ImGui::Button("Restart")) { m_PreviewTime = 0.f; }

        ImGui::SameLine();
        ImGui::TextDisabled("step %u / %u", InData.StepCount() > 0 ? lSample.Step + 1u : 0u,
                            InData.StepCount());

        // Scrubbing pauses the clock.
        const Uint32 lTicks  = InData.TotalTicks();
        const float  lLength = (InData.Fps > 0.f && lTicks > 0u)
                                   ? static_cast<float>(lTicks) / InData.Fps
                                   : 0.f;

        if (lLength > 0.f)
        {
            float lScrub = (m_PreviewTime > lLength) ? std::fmod(m_PreviewTime, lLength) : m_PreviewTime;

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 8.f);
            if (ImGui::SliderFloat("##scrub", &lScrub, 0.f, lLength, "%.2fs"))
            {
                m_PreviewTime     = lScrub;
                m_bPreviewPlaying = false;
            }
        }

        const EditorImage lImage = ResolveStepImage(InData, lSample.Step);

        if (!lImage.IsValid())
        {
            // Say it rather than draw an empty rectangle.
            ImGui::TextDisabled(InData.StepCount() == 0u ? "No steps yet — press Add Step."
                                                         : "This step has no picture to show.");
            return;
        }

        ImguiWidgets::Image(lImage, ImVec2(MAX_PREVIEW_SIZE, MAX_PREVIEW_SIZE));
    }

    EditorImage AnimationClipPanel::ResolveStepImage(const AnimationClipData& InData, const Uint32 InStepIndex)
    {
        const AnimationStep* lStep = InData.StepAt(InStepIndex);

        if (lStep == nullptr) { return EditorImage{}; }

        // Texture clip: the step has its own image.
        if (InData.Sheet.IsEmpty())
        {
            const TextureResource* lTexture = ClaimTexture(lStep->Texture.Path);

            return (lTexture != nullptr && lTexture->GetTexture() != nullptr)
                       ? m_Context.UIBackend.GetTextureImage(*lTexture->GetTexture())
                       : EditorImage{};
        }

        // Sheet clip: the frame's rect, cropped from the sheet's image.
        const SpriteSheetData* lSheet = ClaimSheet(InData);

        if (lSheet == nullptr) { return EditorImage{}; }

        const SpriteFrame* lFrame = nullptr;

        for (Uint32 lIndex = 0; lIndex < lSheet->FrameCount(); ++lIndex)
        {
            if (lSheet->Frames[lIndex].Name == lStep->Frame)
            {
                lFrame = &lSheet->Frames[lIndex];
                break;
            }
        }

        if (lFrame == nullptr) { return EditorImage{}; }

        const TextureResource* lTexture = ClaimTexture(lSheet->Texture.Path);

        if (lTexture == nullptr || lTexture->GetTexture() == nullptr) { return EditorImage{}; }

        return CropTo(m_Context.UIBackend.GetTextureImage(*lTexture->GetTexture()),
                      *lFrame, lTexture->Width, lTexture->Height);
    }

    void AnimationClipPanel::DrawStepList(const AnimationClipData& InData)
    {
        if (!ImGui::TreeNodeEx("Steps", ImGuiTreeNodeFlags_DefaultOpen)) { return; }

        if (ImGui::Button("Add Step")) { ClipOps::AddStep(m_Context); }

        ImGui::SameLine();
        ImGui::BeginDisabled(m_Selected < 0);

        if (ImGui::Button("Remove") && m_Selected >= 0)
        {
            if (ClipOps::RemoveStep(m_Context, static_cast<Uint32>(m_Selected)))
            {
                m_Selected = -1;   // the selected step was removed
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Up") && m_Selected > 0)
        {
            if (ClipOps::MoveStep(m_Context, static_cast<Uint32>(m_Selected), -1)) { --m_Selected; }
        }

        ImGui::SameLine();
        if (ImGui::Button("Down") && m_Selected >= 0)
        {
            if (ClipOps::MoveStep(m_Context, static_cast<Uint32>(m_Selected), 1)) { ++m_Selected; }
        }

        ImGui::EndDisabled();

        // A texture clip is built by dragging textures in (each drop adds a step). Only offered when
        // the clip has no sheet (a sheet clip uses frames).
        if (InData.Sheet.IsEmpty())
        {
            ImGui::Button("Drop a texture here to add a step", ImVec2(-1.f, 0.f));

            if (OpaaxString lDropped;
                AcceptResourceDragPayload(ResourceTypeID::Get<TextureResource>(), lDropped))
            {
                ClipOps::AddStepWithTexture(m_Context, lDropped);
            }
        }

        for (Uint32 lIndex = 0; lIndex < InData.StepCount(); ++lIndex)
        {
            const AnimationStep& lStep = InData.Steps[lIndex];

            const char* lWhat = InData.Sheet.IsEmpty()
                                    ? (lStep.Texture.IsEmpty() ? "(no texture)" : lStep.Texture.Path.CStr())
                                    : (lStep.Frame.IsValid() ? lStep.Frame.CStr() : "(unnamed frame)");

            char lLabel[192];
            std::snprintf(lLabel, sizeof(lLabel), "%u  %s  x%u##step%u",
                          lIndex, lWhat, lStep.EffectiveHold(), lIndex);

            if (ImGui::Selectable(lLabel, m_Selected == static_cast<Int32>(lIndex)))
            {
                m_Selected = static_cast<Int32>(lIndex);
            }
        }

        ImGui::TreePop();
    }

    void AnimationClipPanel::DrawSelectedStep(AnimationClipData& InData)
    {
        if (m_Selected < 0 || static_cast<Uint32>(m_Selected) >= InData.StepCount())
        {
            ImGui::TextDisabled("Select a step to edit it.");
            return;
        }

        const Uint32 lIndex = static_cast<Uint32>(m_Selected);

        ImGui::Text("Step %u", lIndex);

        DrawFramePicker(InData, lIndex);

        ImGui::PushID(static_cast<int>(lIndex));
        DrawProperties(m_Context.Widgets, InData.Steps[lIndex]);
        ImGui::PopID();

        // Same as the settings, kept separate so a settings edit and a step edit are two undo steps.
        const bool lItemActive = ImGui::IsAnyItemActive();

        if (lItemActive && !m_bWasStepItemActive)
        {
            m_StepGesture.Begin(m_Context, lIndex);
            m_bStepGestureOpen = true;
        }
        else if (!lItemActive && m_bWasStepItemActive && m_bStepGestureOpen)
        {
            if (m_StepGesture.End(m_Context)) { m_Context.Undo.Record(Move(m_StepGesture)); }

            m_StepGesture      = ClipStepEdit{};
            m_bStepGestureOpen = false;
        }

        m_bWasStepItemActive = lItemActive;
    }

    void AnimationClipPanel::DrawFramePicker(const AnimationClipData& InData, const Uint32 InStepIndex)
    {
        if (InData.Sheet.IsEmpty()) { return; }   // a texture clip has no frames

        const SpriteSheetData* lSheet = ClaimSheet(InData);

        if (lSheet == nullptr)
        {
            ImGui::TextDisabled("The clip's sheet did not load — cannot pick a frame.");
            return;
        }

        const AnimationStep* lStep = InData.StepAt(InStepIndex);
        const char*          lCurrent = (lStep != nullptr && lStep->Frame.IsValid())
                                            ? lStep->Frame.CStr()
                                            : "(none)";

        if (!ImGui::BeginCombo("Frame##picker", lCurrent)) { return; }

        Uint32 lNamed = 0;

        for (Uint32 lIndex = 0; lIndex < lSheet->FrameCount(); ++lIndex)
        {
            const SpriteFrame& lFrame = lSheet->Frames[lIndex];

            // An unnamed frame cannot be referenced (steps name frames): say so, it is fixed in the sheet editor.
            if (!lFrame.Name.IsValid()) { continue; }

            ++lNamed;

            if (ImGui::Selectable(lFrame.Name.CStr(),
                                  lStep != nullptr && lStep->Frame == lFrame.Name))
            {
                ClipOps::SetStepFrame(m_Context, InStepIndex, lFrame.Name);
            }
        }

        if (lNamed == 0)
        {
            ImGui::TextDisabled("No named frames in this sheet.");
            ImGui::TextDisabled("Open it and press Auto-Name.");
        }

        ImGui::EndCombo();
    }

    // =============================================================================
    // Claims
    // =============================================================================
    const SpriteSheetData* AnimationClipPanel::ClaimSheet(const AnimationClipData& InData)
    {
        if (InData.Sheet.IsEmpty())
        {
            m_SheetClaim       = {};
            m_ClaimedSheetPath = OpaaxString();
            return nullptr;
        }

        if (InData.Sheet.Path != m_ClaimedSheetPath)
        {
            const OpaaxString lAbsolute = m_Context.Paths.AssetToAbsolute(InData.Sheet.Path);

            m_SheetClaim       = m_Context.Resources.Load<SpriteSheetResource>(lAbsolute.CStr());
            m_ClaimedSheetPath = InData.Sheet.Path;

            if (m_SheetClaim.IsValid())
            {
                OPAAX_LOG(LogAnimationClipPanel, Trace, "Clip sheet '{}' loaded — {} frame(s)",
                          lAbsolute.CStr(), m_SheetClaim.Get()->Data.FrameCount());
            }
            else
            {
                OPAAX_LOG(LogAnimationClipPanel, Warn, "Clip sheet '{}' did not load", lAbsolute.CStr());
            }
        }

        // IsValid, not Get() != nullptr: a failed claim returns the placeholder.
        return m_SheetClaim.IsValid() ? &m_SheetClaim.Get()->Data : nullptr;
    }

    const TextureResource* AnimationClipPanel::ClaimTexture(const OpaaxString& InAssetPath)
    {
        if (InAssetPath.IsEmpty())
        {
            m_TextureClaim       = {};
            m_ClaimedTexturePath = OpaaxString();
            return nullptr;
        }

        if (InAssetPath != m_ClaimedTexturePath)
        {
            const OpaaxString lAbsolute = m_Context.Paths.AssetToAbsolute(InAssetPath);

            m_TextureClaim       = m_Context.Resources.Load<TextureResource>(lAbsolute.CStr());
            m_ClaimedTexturePath = InAssetPath;

            if (!m_TextureClaim.IsValid())
            {
                OPAAX_LOG(LogAnimationClipPanel, Warn, "Clip image '{}' did not load", lAbsolute.CStr());
            }
        }

        return m_TextureClaim.IsValid() ? m_TextureClaim.Get() : nullptr;
    }
}
