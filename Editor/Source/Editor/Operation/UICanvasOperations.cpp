#include "Editor/Operation/UICanvasOperations.h"
#include "Editor/Operation/ResourceOperations.h"

#include "Core/Maths/Maths.h"
#include "Core/String/OpaaxStringJson.h"
#include "Core/String/UniqueName.h"
#include "Editor/Clipboard/EditorClipboard.h"

#include "Application/OpaaxApplication.h"
#include "Core/IO/FileIO.h"
#include "Application/Services/IEngine.h"
#include "Editor/EditorContext.h"
#include "Editor/Undo/EditorUndo.h"
#include "Editor/Undo/UICanvasUndoables.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Resources/Asset/AssetResource.h"
#include "UI/UICanvasAsset.h"
#include "UI/UICanvasFile.h"
#include "UI/UIWidget.h"
#include "UI/UIWidgetNames.h"
#include "Object/ObjectClassRegistry.h"

namespace Opaax::Editor
{
    namespace
    {
        const ObjectClassRegistry& Classes()
        {
            return OpaaxApplication::GetAppService<IEngine>().GetRegistries().ObjectClasses();
        }

        /** Records InBefore -> the current tree, under InLabel. */
        void Record(EditorContext& InContext, OpaaxString InBefore, const char* InLabel)
        {
            UITreeEdit lStep;
            lStep.CanvasPath = InContext.UICanvasDocument.AbsPath();
            lStep.Before     = Move(InBefore);
            lStep.After      = InContext.UICanvasDocument.Serialize();
            lStep.LabelText  = InLabel;

            InContext.Undo.Record(Move(lStep));
        }

        /** Whether InMaybeAncestor is a prefix of InPath (the node is inside that subtree). */
        bool IsInside(const UIWidgetPath& InPath, const UIWidgetPath& InMaybeAncestor)
        {
            if (InMaybeAncestor.size() > InPath.size()) { return false; }

            for (Uint64 lIndex = 0; lIndex < InMaybeAncestor.size(); ++lIndex)
            {
                if (InPath[lIndex] != InMaybeAncestor[lIndex]) { return false; }
            }

            return true;
        }
    }

    OpaaxString UICanvasOps::Snapshot(EditorContext& InContext)
    {
        return InContext.UICanvasDocument.Serialize();
    }

    UIWidgetPath UICanvasOps::AddWidget(EditorContext& InContext, const OpaaxStringID InType,
                                        const UIWidgetPath& InParent)
    {
        if (!InContext.UICanvasDocument.IsOpen()) { return {}; }

        UIWidget* const lParent = InContext.UICanvasDocument.Resolve(InParent);
        if (lParent == nullptr)
        {
            OPAAX_LOG(LogEditorUICanvasDocument, Warn, "Add — the parent is no longer in the tree");
            return {};
        }

        TStrongObjectPtr<UIWidget> lWidget = UIWidget::Create(InType, Classes());
        if (!lWidget)
        {
            OPAAX_LOG(LogEditorUICanvasDocument, Warn, "Add — '{}' is not a registered widget type", InType);
            return {};
        }

        OpaaxString lBefore = Snapshot(InContext);

        // A new widget is named after its type, numbered when the canvas has one already.
        lWidget->Name = InType.ToString();
        UIWidgetNames::NameUniquely(InContext.UICanvasDocument.GetCanvas().Root(), *lWidget);

        UIWidget* const lAdded = lParent->AddChild(Move(lWidget));
        Record(InContext, Move(lBefore), "Add Widget");

        return InContext.UICanvasDocument.PathOf(*lAdded);
    }

    bool UICanvasOps::RemoveWidget(EditorContext& InContext, const UIWidgetPath& InPath)
    {
        if (!InContext.UICanvasDocument.IsOpen()) { return false; }

        if (InPath.empty())
        {
            OPAAX_LOG(LogEditorUICanvasDocument, Warn, "Delete — the root is the canvas itself and stays");
            return false;
        }

        UIWidget* const lWidget = InContext.UICanvasDocument.Resolve(InPath);
        if (lWidget == nullptr || lWidget->GetParent() == nullptr) { return false; }

        OpaaxString lBefore = Snapshot(InContext);

        lWidget->GetParent()->RemoveChild(*lWidget);
        InContext.UICanvasDocument.ClearSelection();

        Record(InContext, Move(lBefore), "Delete Widget");
        return true;
    }

    bool UICanvasOps::ReparentWidget(EditorContext& InContext, const UIWidgetPath& InPath,
                                     const UIWidgetPath& InNewParent)
    {
        if (!InContext.UICanvasDocument.IsOpen() || InPath.empty()) { return false; }

        // Moving a node into its own subtree would create a cycle: refused.
        if (IsInside(InNewParent, InPath))
        {
            OPAAX_LOG(LogEditorUICanvasDocument, Warn, "Reparent — a widget cannot move inside itself");
            return false;
        }

        UIWidget* const lWidget = InContext.UICanvasDocument.Resolve(InPath);
        UIWidget* const lParent = InContext.UICanvasDocument.Resolve(InNewParent);

        if (lWidget == nullptr || lParent == nullptr || lWidget->GetParent() == nullptr) { return false; }
        if (lWidget->GetParent() == lParent) { return false; }   // already there

        OpaaxString lBefore = Snapshot(InContext);

        TStrongObjectPtr<UIWidget> lOwned = lWidget->GetParent()->RemoveChild(*lWidget);
        UIWidget* const      lMoved = lParent->AddChild(Move(lOwned));

        InContext.UICanvasDocument.Select(InContext.UICanvasDocument.PathOf(*lMoved));

        Record(InContext, Move(lBefore), "Reparent Widget");
        return true;
    }

    bool UICanvasOps::MoveWidget(EditorContext& InContext, const UIWidgetPath& InPath, const Int32 InDelta)
    {
        if (!InContext.UICanvasDocument.IsOpen() || InPath.empty() || InDelta == 0) { return false; }

        UIWidget* const lWidget = InContext.UICanvasDocument.Resolve(InPath);
        if (lWidget == nullptr || lWidget->GetParent() == nullptr) { return false; }

        UIWidget& lParent = *lWidget->GetParent();
        const Int64 lCount = static_cast<Int64>(lParent.GetChildren().size());
        const Int64 lFrom  = static_cast<Int64>(InPath.back());
        const Int64 lTo    = Maths::Clamp(lFrom + InDelta, Int64{ 0 }, lCount - 1);

        if (lTo == lFrom) { return false; }   // already at that end

        OpaaxString lBefore = Snapshot(InContext);

        TStrongObjectPtr<UIWidget> lOwned = lParent.RemoveChild(*lWidget);
        UIWidget* const      lMoved = lParent.AddChild(Move(lOwned), static_cast<Uint64>(lTo));

        InContext.UICanvasDocument.Select(InContext.UICanvasDocument.PathOf(*lMoved));

        Record(InContext, Move(lBefore), InDelta < 0 ? "Move Widget Up" : "Move Widget Down");
        return true;
    }

    UIWidgetPath UICanvasOps::DuplicateWidget(EditorContext& InContext, const UIWidgetPath& InPath)
    {
        if (!InContext.UICanvasDocument.IsOpen() || InPath.empty()) { return {}; }

        UIWidget* const lWidget = InContext.UICanvasDocument.Resolve(InPath);
        if (lWidget == nullptr || lWidget->GetParent() == nullptr) { return {}; }

        TStrongObjectPtr<UIWidget> lClone = UICanvasFile::CloneWidget(*lWidget, Classes());
        if (!lClone) { return {}; }

        OpaaxString lBefore = Snapshot(InContext);

        // The whole copy: a name its original keeps would make FindByName ambiguous.
        UIWidgetNames::NameUniquely(InContext.UICanvasDocument.GetCanvas().Root(), *lClone);

        UIWidget* const lAdded = lWidget->GetParent()->AddChild(Move(lClone), InPath.back() + 1);
        Record(InContext, Move(lBefore), "Duplicate Widget");

        return InContext.UICanvasDocument.PathOf(*lAdded);
    }

    OpaaxString UICanvasOps::CopyWidget(EditorContext& InContext, const UIWidgetPath& InPath)
    {
        if (!InContext.UICanvasDocument.IsOpen() || InPath.empty()) { return {}; }

        const UIWidget* const lWidget = InContext.UICanvasDocument.Resolve(InPath);
        return lWidget != nullptr ? UICanvasFile::SerializeNode(*lWidget) : OpaaxString();
    }

    UIWidgetPath UICanvasOps::PasteWidget(EditorContext& InContext, const OpaaxString& InText,
                                          const UIWidgetPath& InParent)
    {
        if (!InContext.UICanvasDocument.IsOpen() || InText.IsEmpty()) { return {}; }

        UIWidget* const lParent = InContext.UICanvasDocument.Resolve(InParent);
        if (lParent == nullptr) { return {}; }

        TStrongObjectPtr<UIWidget> lWidget = UICanvasFile::DeserializeNode(InText, Classes());
        if (!lWidget) { return {}; }   // not a widget node

        OpaaxString lBefore = Snapshot(InContext);

        UIWidgetNames::NameUniquely(InContext.UICanvasDocument.GetCanvas().Root(), *lWidget);

        UIWidget* const lAdded = lParent->AddChild(Move(lWidget));
        Record(InContext, Move(lBefore), "Paste Widget");

        return InContext.UICanvasDocument.PathOf(*lAdded);
    }

    bool UICanvasOps::CopySelected(EditorContext& InContext)
    {
        const OpaaxString lText = CopyWidget(InContext, InContext.UICanvasDocument.SelectedPath());
        if (lText.IsEmpty()) { return false; }   // nothing selected, or the root

        const nlohmann::json lNode = nlohmann::json::parse(ToJsonString(lText), nullptr, false);
        if (lNode.is_discarded()) { return false; }

        InContext.Clipboard.Copy(EClipboardKind::UIWidget, lNode);
        return true;
    }

    bool UICanvasOps::CutSelected(EditorContext& InContext)
    {
        const UIWidgetPath lSelected = InContext.UICanvasDocument.SelectedPath();
        return !lSelected.empty() && CopySelected(InContext) && RemoveWidget(InContext, lSelected);
    }

    UIWidgetPath UICanvasOps::PasteFromClipboard(EditorContext& InContext, OpaaxString* OutError)
    {
        nlohmann::json lNode;
        OpaaxString    lError;
        OpaaxString    lText;
        if (InContext.Clipboard.Read(EClipboardKind::UIWidget, lNode, lError))
        {
            lText = ToJsonText(lNode);
        }
        else
        {
            // A bare node (text copied before the clipboard had kinds) still pastes; PasteWidget refuses
            // anything else. Another kind of editor copy (entities...) is refused here.
            lText = InContext.Clipboard.GetText();

            const nlohmann::json lJson = nlohmann::json::parse(ToJsonString(lText), nullptr, false);
            if (lJson.is_object() && lJson.contains(ClipboardPayload::KEY_KIND))
            {
                if (OutError != nullptr) { *OutError = lError; }
                return {};
            }
        }

        const UIWidgetPath lPath = PasteWidget(InContext, lText, InContext.UICanvasDocument.SelectedPath());
        if (lPath.empty())
        {
            if (OutError != nullptr) { *OutError = lError.IsEmpty() ? OpaaxString("the clipboard holds no widget") : lError; }
            return lPath;
        }

        InContext.UICanvasDocument.Select(lPath);
        return lPath;
    }

    UIWidgetPath UICanvasOps::DuplicateSelected(EditorContext& InContext)
    {
        const UIWidgetPath lPath = DuplicateWidget(InContext, InContext.UICanvasDocument.SelectedPath());
        if (!lPath.empty()) { InContext.UICanvasDocument.Select(lPath); }
        return lPath;
    }

    bool UICanvasOps::CommitEdit(EditorContext& InContext, const OpaaxString& InBefore, const char* InLabel)
    {
        if (!InContext.UICanvasDocument.IsOpen()) { return false; }

        // A rename onto another widget's name gets the next free one.
        const UIWidget& lRoot = InContext.UICanvasDocument.GetCanvas().Root();
        if (UIWidget* const lEdited = InContext.UICanvasDocument.Resolve(InContext.UICanvasDocument.SelectedPath());
            lEdited != nullptr && !lEdited->Name.IsEmpty() && UIWidgetNames::IsTaken(lRoot, lEdited->Name, lEdited))
        {
            const OpaaxString lTaken = lEdited->Name;
            lEdited->Name = UniqueName::Make(lTaken, [&lRoot, lEdited](const OpaaxString& InCandidate)
            {
                return UIWidgetNames::IsTaken(lRoot, InCandidate, lEdited);
            });
            OPAAX_LOG(LogEditorUICanvasDocument, Warn, "'{}' is taken in this canvas, named '{}' instead",
                      lTaken.CStr(), lEdited->Name.CStr());
        }

        if (Snapshot(InContext) == InBefore)
        {
            return false;   // a gesture that changed nothing is not a step
        }

        Record(InContext, InBefore, InLabel != nullptr ? InLabel : "Edit Widget");
        return true;
    }

    bool UICanvasOps::Save(EditorContext& InContext)
    {
        if (!InContext.UICanvasDocument.IsOpen()) { return false; }

        const OpaaxString& lPath = InContext.UICanvasDocument.AbsPath();

        if (!FileIO::WriteAllText(lPath, InContext.UICanvasDocument.Serialize()))
        {
            OPAAX_LOG(LogEditorUICanvasDocument, Error, "Cannot write canvas '{}'", lPath.CStr());
            return false;
        }

        InContext.UICanvasDocument.MarkSaved();

        // Reload the resource, so a running game using this canvas sees the change.
        ResourceOps::SavedToDisk<AssetResource>(InContext, lPath, UICanvasAsset::StaticClass().GetName());

        OPAAX_LOG(LogEditorUICanvasDocument, Info, "Saved '{}'", lPath.CStr());
        return true;
    }
}
