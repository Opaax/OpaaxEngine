#include "Editor/Operation/FontFamilyOperations.h"
#include "Editor/Operation/ResourceOperations.h"

#include "Editor/EditorContext.h"
#include "Editor/EditorFontFamilyDocument.h"
#include "Editor/Undo/EditorUndo.h"
#include "Editor/Undo/FontFamilyUndoables.h"

#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Engine/Subsystems/Resources/Types/Font/FontFamilyFile.h"
#include "Engine/Subsystems/Resources/Types/Font/FontFamilyResource.h"

namespace Opaax::Editor
{
    namespace
    {
        /** Record a whole-list replacement under InLabel, and publish it into the document. */
        void RecordEntries(EditorContext& InContext, TDynArray<FontFamilyEntry> InAfter, const char* InLabel)
        {
            FontFamilyData& lData = InContext.FamilyDocument.GetMutableData();

            FontFamilyEntriesEdit lStep;
            lStep.FamilyPath = InContext.FamilyDocument.AbsPath();
            lStep.Before     = lData.Entries;
            lStep.After      = InAfter;
            lStep.LabelText  = InLabel;

            lData.Entries = Move(InAfter);

            InContext.Undo.Record(Move(lStep));
        }

        /** Whether any entry OTHER than InSkip already answers to InStyle. */
        bool StyleTakenByOther(const FontFamilyData& InData, const FontStyleKey& InStyle, const Uint32 InSkip)
        {
            for (Uint32 lIndex = 0; lIndex < InData.EntryCount(); ++lIndex)
            {
                if (lIndex != InSkip && InData.Entries[lIndex].Style == InStyle) { return true; }
            }

            return false;
        }
    }

    bool FamilyOps::AddEntry(EditorContext& InContext, const FontStyleKey& InStyle)
    {
        if (!InContext.FamilyDocument.IsOpen()) { return false; }

        const FontFamilyData& lData = InContext.FamilyDocument.GetData();

        if (lData.FindExact(InStyle) != nullptr)
        {
            OPAAX_LOG(LogEditorFontFamilyDocument, Warn,
                      "This family already has a {}/{}/{}/{} face", ToString(InStyle.Subset),
                      ToString(InStyle.Weight), ToString(InStyle.Width), ToString(InStyle.Slant));
            return false;
        }

        TDynArray<FontFamilyEntry> lAfter = lData.Entries;

        FontFamilyEntry lNew;
        lNew.Style = InStyle;
        lAfter.emplace_back(Move(lNew));

        RecordEntries(InContext, Move(lAfter), "Add Face");
        return true;
    }

    bool FamilyOps::RemoveEntry(EditorContext& InContext, const Uint32 InIndex)
    {
        if (!InContext.FamilyDocument.IsOpen()) { return false; }

        const FontFamilyData& lData = InContext.FamilyDocument.GetData();

        if (InIndex >= lData.EntryCount()) { return false; }

        TDynArray<FontFamilyEntry> lAfter = lData.Entries;
        lAfter.erase(lAfter.begin() + InIndex);

        RecordEntries(InContext, Move(lAfter), "Remove Face");
        return true;
    }

    bool FamilyOps::CommitEntryEdit(EditorContext& InContext, const Uint32 InIndex,
                                    const FontFamilyEntry& InBefore)
    {
        if (!InContext.FamilyDocument.IsOpen()) { return false; }

        FontFamilyData& lData = InContext.FamilyDocument.GetMutableData();

        if (InIndex >= lData.EntryCount()) { return false; }

        FontFamilyEntry& lEntry = lData.Entries[InIndex];

        // REVERTED, not merely refused: the drawer already wrote the duplicate into the document, so
        // leaving it would ship a family where one face can never be resolved.
        if (StyleTakenByOther(lData, lEntry.Style, InIndex))
        {
            OPAAX_LOG(LogEditorFontFamilyDocument, Warn,
                      "{}/{}/{}/{} is already in this family — the edit was reverted",
                      ToString(lEntry.Style.Subset), ToString(lEntry.Style.Weight),
                      ToString(lEntry.Style.Width),  ToString(lEntry.Style.Slant));

            lEntry = InBefore;
            return false;
        }

        if (lEntry.Style == InBefore.Style && lEntry.Face.Path == InBefore.Face.Path)
        {
            return false;   // a gesture that changed nothing is not a step
        }

        TDynArray<FontFamilyEntry> lAfter  = lData.Entries;
        TDynArray<FontFamilyEntry> lBefore = lData.Entries;
        lBefore[InIndex] = InBefore;

        FontFamilyEntriesEdit lStep;
        lStep.FamilyPath = InContext.FamilyDocument.AbsPath();
        lStep.Before     = Move(lBefore);
        lStep.After      = Move(lAfter);
        lStep.LabelText  = "Edit Face";

        InContext.Undo.Record(Move(lStep));
        return true;
    }

    bool FamilyOps::Save(EditorContext& InContext)
    {
        if (!InContext.FamilyDocument.IsOpen()) { return false; }

        if (!FontFamilyFile::Save(InContext.FamilyDocument.AbsPath(), InContext.FamilyDocument.GetData()))
        {
            return false;   // FontFamilyFile logged why
        }

        InContext.FamilyDocument.MarkSaved();

        // AND PUBLISH IT ([[L75]]): without this a TextComponent already holding this family keeps
        // resolving styles against the first parse.
        ResourceOps::SavedToDisk<FontFamilyResource>(InContext, InContext.FamilyDocument.AbsPath());

        return true;
    }
}
