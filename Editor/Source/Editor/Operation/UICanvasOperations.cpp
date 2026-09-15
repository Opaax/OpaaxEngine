#include "Editor/Operation/UICanvasOperations.h"
#include "Editor/Operation/ResourceOperations.h"

#include <algorithm>   // std::clamp — the reorder's bounds

#include "Application/OpaaxApplication.h"
#include "Core/IO/FileIO.h"
#include "Application/Services/IEngine.h"
#include "Editor/EditorContext.h"
#include "Editor/Undo/EditorUndo.h"
#include "Editor/Undo/UICanvasUndoables.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Engine/Subsystems/Resources/Types/UI/UICanvasResource.h"
#include "UI/UICanvasFile.h"
#include "UI/UIWidget.h"
#include "UI/UIWidgetRegistry.h"

namespace Opaax::Editor
{
    namespace
    {
        const UIWidgetRegistry& Registry()
        {
            return OpaaxApplication::GetAppService<IEngine>().GetRegistries().UIWidgets();
        }

        /** Record InBefore -> the tree as it is now, under InLabel. */
        void Record(EditorContext& InContext, OpaaxString InBefore, const char* InLabel)
        {
            UITreeEdit lStep;
            lStep.CanvasPath = InContext.UICanvasDocument.AbsPath();
            lStep.Before     = Move(InBefore);
            lStep.After      = InContext.UICanvasDocument.Serialize();
            lStep.LabelText  = InLabel;

            InContext.Undo.Record(Move(lStep));
        }

        /** Whether InMaybeAncestor is InPath's prefix — "is this node inside that subtree". */
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

        TUniquePtr<UIWidget> lWidget = Registry().Create(InType);
        if (!lWidget)
        {
            OPAAX_LOG(LogEditorUICanvasDocument, Warn, "Add — '{}' is not a registered widget type", InType);
            return {};
        }

        OpaaxString lBefore = Snapshot(InContext);

        // A fresh widget is named after its type, so the tree reads before anything is authored.
        lWidget->Name = InType.ToString();

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

        // INTO ITS OWN SUBTREE is a cycle: the moved node would be its own ancestor and no walk
        // would terminate. Refused here rather than survived downstream (**HR**'s rule).
        if (IsInside(InNewParent, InPath))
        {
            OPAAX_LOG(LogEditorUICanvasDocument, Warn, "Reparent — a widget cannot move inside itself");
            return false;
        }

        UIWidget* const lWidget = InContext.UICanvasDocument.Resolve(InPath);
        UIWidget* const lParent = InContext.UICanvasDocument.Resolve(InNewParent);

        if (lWidget == nullptr || lParent == nullptr || lWidget->GetParent() == nullptr) { return false; }
        if (lWidget->GetParent() == lParent) { return false; }   // already there — not a step

        OpaaxString lBefore = Snapshot(InContext);

        TUniquePtr<UIWidget> lOwned = lWidget->GetParent()->RemoveChild(*lWidget);
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
        const Int64 lTo    = std::clamp(lFrom + InDelta, Int64{ 0 }, lCount - 1);

        if (lTo == lFrom) { return false; }   // already at the end it was pushed toward

        OpaaxString lBefore = Snapshot(InContext);

        TUniquePtr<UIWidget> lOwned = lParent.RemoveChild(*lWidget);
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

        TUniquePtr<UIWidget> lClone = UICanvasFile::CloneWidget(*lWidget, Registry());
        if (!lClone) { return {}; }

        OpaaxString lBefore = Snapshot(InContext);

        // A same-named twin would shadow the original for FindByName; the suffix is Unity's.
        lClone->Name = lWidget->Name + " (1)";

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

        TUniquePtr<UIWidget> lWidget = UICanvasFile::DeserializeNode(InText, Registry());
        if (!lWidget) { return {}; }   // not a node — the clipboard held something else

        OpaaxString lBefore = Snapshot(InContext);

        UIWidget* const lAdded = lParent->AddChild(Move(lWidget));
        Record(InContext, Move(lBefore), "Paste Widget");

        return InContext.UICanvasDocument.PathOf(*lAdded);
    }

    bool UICanvasOps::CommitEdit(EditorContext& InContext, const OpaaxString& InBefore, const char* InLabel)
    {
        if (!InContext.UICanvasDocument.IsOpen()) { return false; }

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

        // AND PUBLISH IT ([[L75]]): without this a running game holding this canvas keeps the tree
        // it built from the first parse, so a Save would not reach the HUD on screen.
        ResourceOps::SavedToDisk<UICanvasResource>(InContext, lPath);

        OPAAX_LOG(LogEditorUICanvasDocument, Info, "Saved '{}'", lPath.CStr());
        return true;
    }
}
