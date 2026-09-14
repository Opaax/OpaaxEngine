#include "Editor/Operation/UICanvasOperations.h"
#include "Editor/Operation/ResourceOperations.h"

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
