#include "Editor/Assets/AssetEditors.h"

#include <imgui.h>

#include "Application/Services/IEngine.h"
#include "Application/Services/IPaths.h"
#include "Core/String/OpaaxStringJson.h"
#include "Editor/Assets/AssetEdit.h"
#include "Editor/Assets/PropertyAssetEditor.h"
#include "Editor/EditorContext.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/Operation/ResourceOperations.h"
#include "Editor/UI/IEditorDialogs.h"
#include "Editor/UI/IEditorGui.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Core/String/OpaaxPathString.h"
#include "Platform/IFileSystem.h"
#include "Resources/Asset/AssetResource.h"
#include "Resources/ResourceFormat.h"   // NormalizeExtension

namespace Opaax::Editor
{
    AssetEditors::AssetEditors()  = default;
    AssetEditors::~AssetEditors() = default;

    // =============================================================================
    // Documents
    // =============================================================================

    bool AssetEditors::Open(const OpaaxString& InAbsPath, OpaaxString* OutError)
    {
        if (OpenAsset* lOpen = FindOpen(InAbsPath))
        {
            lOpen->bFocusNext = true;
            return true;
        }

        OpaaxString               lError;
        TUniquePtr<AssetDocument> lDocument =
            AssetDocument::Open(InAbsPath, m_Context->Engine.GetRegistries().ObjectClasses(), lError);

        if (lDocument == nullptr)
        {
            OPAAX_LOG(LogAssetEditors, Error, "Cannot open asset '{}': {}", InAbsPath.CStr(), lError.CStr());
            if (OutError != nullptr) { *OutError = Move(lError); }
            return false;
        }

        OPAAX_LOG(LogAssetEditors, Info, "Opened asset '{}' ({})", InAbsPath.CStr(), lDocument->GetClassName());
        AddWindow(Move(lDocument));
        return true;
    }

    bool AssetEditors::Create(const OpaaxString& InAbsPath, const ObjectClass& InClass, OpaaxString* OutError)
    {
        const auto lFail = [&InAbsPath, OutError](OpaaxString InWhy)
        {
            OPAAX_LOG(LogAssetEditors, Warn, "No new asset at '{}': {}", InAbsPath.CStr(), InWhy.CStr());
            if (OutError != nullptr) { *OutError = Move(InWhy); }
            return false;
        };

        if (m_Context->Paths.AbsoluteToAsset(InAbsPath).IsEmpty())
        {
            return lFail(OpaaxString("it is outside the project's assets (a game loads only its own)"));
        }

        if (PathString::Stem(InAbsPath).IsEmpty()
            || NormalizeExtension(PathString::Extension(InAbsPath)) != NormalizeExtension(AssetFile::ASSET_EXTENSION))
        {
            return lFail(OpaaxString::Format("an asset file is named <name>{}", AssetFile::ASSET_EXTENSION));
        }

        // New means new: an existing file is opened, never overwritten.
        if (m_Context->FileSystem.IsPathExist(InAbsPath) || FindOpen(InAbsPath) != nullptr)
        {
            return lFail(OpaaxString("that file already exists: open it instead"));
        }

        TUniquePtr<AssetDocument> lDocument = AssetDocument::Create(InAbsPath, InClass);
        if (lDocument == nullptr)
        {
            return lFail(OpaaxString::Format("'{}' is not a class an asset can be made of", InClass.GetName().CStr()));
        }

        if (!lDocument->Save())
        {
            return lFail(OpaaxString("the file could not be written (see the log)"));
        }

        ResourceOps::SavedToDisk<AssetResource>(*m_Context, InAbsPath);

        OPAAX_LOG(LogAssetEditors, Info, "Created asset '{}' ({})", InAbsPath.CStr(), InClass.GetName());
        AddWindow(Move(lDocument));
        return true;
    }

    bool AssetEditors::Close(const OpaaxString& InAbsPath, const bool bInDiscard)
    {
        for (Uint64 lIndex = 0; lIndex < m_Open.size(); ++lIndex)
        {
            AssetDocument& lDocument = *m_Open[lIndex]->Document;
            if (lDocument.GetAbsPath() != InAbsPath)
            {
                continue;
            }

            if (lDocument.IsDirty() && !bInDiscard)
            {
                bool bLose = false;
                m_Context->Dialogs.Confirm(OpaaxString("Unsaved asset"),
                                           OpaaxString::Format("'{}' has unsaved changes. Close it and lose them?",
                                                               lDocument.GetFileName().CStr()),
                                           [&bLose](const EDialogAnswer InAnswer) { bLose = InAnswer == EDialogAnswer::Yes; });
                if (!bLose)
                {
                    return false;
                }
            }

            OPAAX_LOG(LogAssetEditors, Info, "Closed asset '{}'", InAbsPath.CStr());

            if (m_FocusedPath == InAbsPath) { m_FocusedPath = OpaaxString(); }
            m_Open.erase(m_Open.begin() + static_cast<PtrDiff>(lIndex));
            return true;
        }

        return true;
    }

    void AssetEditors::CloseAll()
    {
        m_Open.clear();
        m_FocusedPath = OpaaxString();
    }

    bool AssetEditors::Save(const OpaaxString& InAbsPath)
    {
        OpenAsset* lOpen = FindOpen(InAbsPath);
        if (lOpen == nullptr)
        {
            OPAAX_LOG(LogAssetEditors, Warn, "Save ignored — '{}' is not open", InAbsPath.CStr());
            return false;
        }

        if (!lOpen->Document->Save())
        {
            return false;   // the document logged why
        }

        // The running copy follows: a game reading this asset sees the new values.
        ResourceOps::SavedToDisk<AssetResource>(*m_Context, InAbsPath);
        return true;
    }

    Uint32 AssetEditors::SaveAll()
    {
        Uint32 lSaved = 0;

        for (const TUniquePtr<OpenAsset>& lOpen : m_Open)
        {
            if (lOpen->Document->IsClassKnown() && lOpen->Document->IsDirty() && Save(lOpen->Document->GetAbsPath()))
            {
                ++lSaved;
            }
        }

        return lSaved;
    }

    void AssetEditors::Undo(const OpaaxString& InAbsPath)
    {
        if (OpenAsset* lOpen = FindOpen(InAbsPath))
        {
            lOpen->History.Undo(*m_Context);
        }
    }

    void AssetEditors::Redo(const OpaaxString& InAbsPath)
    {
        if (OpenAsset* lOpen = FindOpen(InAbsPath))
        {
            lOpen->History.Redo(*m_Context);
        }
    }

    bool AssetEditors::Edit(const OpaaxString& InAbsPath, const nlohmann::json& InProperties, OpaaxString* OutError)
    {
        OpenAsset* lOpen = FindOpen(InAbsPath);
        if (lOpen == nullptr)
        {
            if (OutError != nullptr) { *OutError = OpaaxString("the asset is not open"); }
            return false;
        }

        // Anything changed in the window and not recorded yet stays its own step.
        RecordChanges(*lOpen);

        const nlohmann::json lBefore = lOpen->Recorded;
        if (!lOpen->Document->SetProperties(InProperties, OutError))
        {
            return false;
        }

        lOpen->Document->GetAsset()->PostEditChange();

        nlohmann::json lAfter = lOpen->Document->GetProperties();
        if (lAfter != lBefore)
        {
            AssetEdit lStep;
            lStep.AssetPath = InAbsPath;
            lStep.Before    = lBefore;
            lStep.After     = lAfter;
            lOpen->History.Record(Move(lStep));
        }

        lOpen->Recorded = Move(lAfter);
        return true;
    }

    void AssetEditors::ApplyFromHistory(const OpaaxString& InAbsPath, const nlohmann::json& InProperties)
    {
        OpenAsset* lOpen = FindOpen(InAbsPath);
        if (lOpen == nullptr)
        {
            OPAAX_LOG(LogAssetEditors, Warn, "Undo step skipped — its asset '{}' is not open", InAbsPath.CStr());
            return;
        }

        OpaaxString lError;
        if (!lOpen->Document->SetProperties(InProperties, &lError))
        {
            OPAAX_LOG(LogAssetEditors, Error, "Undo step on '{}' failed: {}", InAbsPath.CStr(), lError.CStr());
        }

        lOpen->Recorded = lOpen->Document->GetProperties();
    }

    // =============================================================================
    // Get
    // =============================================================================

    AssetDocument* AssetEditors::Find(const OpaaxString& InAbsPath) const
    {
        const OpenAsset* lOpen = FindOpen(InAbsPath);
        return lOpen != nullptr ? lOpen->Document.get() : nullptr;
    }

    const EditorUndo* AssetEditors::FindHistory(const OpaaxString& InAbsPath) const
    {
        const OpenAsset* lOpen = FindOpen(InAbsPath);
        return lOpen != nullptr ? &lOpen->History : nullptr;
    }

    AssetDocument* AssetEditors::GetFocused() const
    {
        return m_FocusedPath.IsEmpty() ? nullptr : Find(m_FocusedPath);
    }

    TDynArray<AssetDocument*> AssetEditors::GetOpen() const
    {
        TDynArray<AssetDocument*> lDocuments;
        for (const TUniquePtr<OpenAsset>& lOpen : m_Open)
        {
            lDocuments.emplace_back(lOpen->Document.get());
        }
        return lDocuments;
    }

    bool AssetEditors::HasUnsaved() const
    {
        for (const TUniquePtr<OpenAsset>& lOpen : m_Open)
        {
            if (lOpen->Document->IsDirty()) { return true; }
        }
        return false;
    }

    AssetEditors::OpenAsset* AssetEditors::FindOpen(const OpaaxString& InAbsPath) const
    {
        for (const TUniquePtr<OpenAsset>& lOpen : m_Open)
        {
            if (lOpen->Document->GetAbsPath() == InAbsPath) { return lOpen.get(); }
        }
        return nullptr;
    }

    void AssetEditors::AddWindow(TUniquePtr<AssetDocument> InDocument)
    {
        TUniquePtr<OpenAsset> lOpen = MakeUnique<OpenAsset>();
        lOpen->Recorded = InDocument->GetProperties();

        if (const Asset* lAsset = InDocument->GetAsset())
        {
            // The nearest registered editor of the class's chain, else the property list.
            const FAssetEditorFactory* lFactory = m_Context->Extensions.AssetTypes().EditorOf(lAsset->GetClass());
            lOpen->Editor = (lFactory != nullptr) ? (*lFactory)(*m_Context) : nullptr;

            if (lOpen->Editor == nullptr)
            {
                lOpen->Editor = MakeUnique<PropertyAssetEditor>(*m_Context);
            }
        }

        lOpen->Document = Move(InDocument);
        m_Open.emplace_back(Move(lOpen));
    }

    void AssetEditors::RecordChanges(OpenAsset& InOpen)
    {
        // A drag or a text being typed: one step once it ends.
        if (ImGui::IsAnyItemActive())
        {
            return;
        }

        nlohmann::json lNow = InOpen.Document->GetProperties();
        if (lNow == InOpen.Recorded)
        {
            return;
        }

        // The class may fix up or refuse what was just changed.
        InOpen.Document->GetAsset()->PostEditChange();

        lNow = InOpen.Document->GetProperties();
        if (lNow == InOpen.Recorded)
        {
            return;
        }

        AssetEdit lStep;
        lStep.AssetPath = InOpen.Document->GetAbsPath();
        lStep.Before    = InOpen.Recorded;
        lStep.After     = lNow;

        InOpen.Recorded = Move(lNow);
        InOpen.History.Record(Move(lStep));
    }

    // =============================================================================
    // Drawing
    // =============================================================================

    void AssetEditors::Draw(IEditorGui& InGui)
    {
        m_FocusedPath = OpaaxString();

        for (const TUniquePtr<OpenAsset>& lOpen : m_Open)
        {
            DrawWindow(InGui, *lOpen);
        }

        // After the loop: a window is not removed while the windows are drawn.
        TDynArray<OpaaxString> lToClose;
        for (const TUniquePtr<OpenAsset>& lOpen : m_Open)
        {
            if (lOpen->bWantClose)
            {
                lOpen->bWantClose = false;
                lToClose.emplace_back(lOpen->Document->GetAbsPath());
            }
        }

        for (const OpaaxString& lPath : lToClose)
        {
            Close(lPath);
        }
    }

    void AssetEditors::DrawWindow(IEditorGui& InGui, OpenAsset& InOpen)
    {
        AssetDocument& lDocument = *InOpen.Document;

        PanelWindowStyle lStyle;
        if (InOpen.Editor != nullptr) { lStyle.DefaultSize = InOpen.Editor->GetDefaultSize(); }

        if (InOpen.bFocusNext)
        {
            ImGui::SetNextWindowFocus();
            InOpen.bFocusNext = false;
        }

        // A new asset window joins the dock node of the last one, as a tab.
        if (m_LastDockId != 0)
        {
            ImGui::SetNextWindowDockID(static_cast<ImGuiID>(m_LastDockId), ImGuiCond_FirstUseEver);
        }

        // The label shows the name and the dirty mark; the id after ### stays the path.
        const OpaaxString lLabel = OpaaxString::Format("{}{}###Asset:{}", lDocument.GetFileName().CStr(),
                                                       lDocument.IsDirty() ? " *" : "", lDocument.GetAbsPath().CStr());

        bool       bWantOpen = true;
        const bool bVisible  = InGui.BeginPanelWindow(lLabel.CStr(), lStyle, bWantOpen);

        if (InGui.IsPanelWindowFocused())
        {
            m_FocusedPath = lDocument.GetAbsPath();
        }

        if (ImGui::GetWindowDockID() != 0)
        {
            m_LastDockId = ImGui::GetWindowDockID();
        }

        if (bVisible)
        {
            ImGui::PushID(lDocument.GetAbsPath().CStr());
            DrawHeader(InOpen);
            ImGui::Separator();

            if (lDocument.IsClassKnown())
            {
                InOpen.Editor->DrawContents(lDocument);
            }
            else
            {
                ImGui::TextWrapped("No module registered the class '%s', so this asset is read-only. Register it "
                                   "with OPAAX_REGISTER_OBJECT in the game module.", lDocument.GetClassName().CStr());
                ImGui::Separator();
                ImGui::TextUnformatted(ToJsonText(lDocument.GetProperties(), 4).CStr());
            }
            ImGui::PopID();
        }

        InGui.EndPanelWindow();

        if (bVisible && lDocument.IsClassKnown())
        {
            RecordChanges(InOpen);
        }

        if (!bWantOpen)
        {
            InOpen.bWantClose = true;
        }
    }

    void AssetEditors::DrawHeader(OpenAsset& InOpen)
    {
        AssetDocument&           lDocument = *InOpen.Document;
        const AssetTypeRegistry& lTypes    = m_Context->Extensions.AssetTypes();
        const bool               bKnown    = lDocument.IsClassKnown();
        const bool               bDirty    = lDocument.IsDirty();

        const OpaaxString lClassLabel = bKnown ? lTypes.LabelOf(lDocument.GetAsset()->GetClass())
                                               : lDocument.GetClassName().ToString();

        ImGui::TextUnformatted(lClassLabel.CStr());
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", m_Context->Paths.AbsoluteToAsset(lDocument.GetAbsPath()).CStr());
        }

        ImGui::SameLine();
        ImGui::BeginDisabled(!bKnown || !bDirty);
        if (ImGui::SmallButton("Save"))
        {
            Save(lDocument.GetAbsPath());
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(!InOpen.History.CanUndo());
        if (ImGui::SmallButton("Undo"))
        {
            Undo(lDocument.GetAbsPath());
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(!InOpen.History.CanRedo());
        if (ImGui::SmallButton("Redo"))
        {
            Redo(lDocument.GetAbsPath());
        }
        ImGui::EndDisabled();
    }
}
