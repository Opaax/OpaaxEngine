#include "Editor/Operation/InputOperations.h"

#include "Editor/EditorContext.h"
#include "Editor/Resources/Types/Input/EditorInputActionDocument.h"
#include "Editor/Resources/Types/Input/EditorInputMappingContextDocument.h"
#include "Editor/Undo/EditorUndo.h"
#include "Editor/Undo/InputUndoables.h"

#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Engine/Subsystems/Resources/Types/Input/InputActionFile.h"
#include "Engine/Subsystems/Resources/Types/Input/InputActionResource.h"
#include "Engine/Subsystems/Resources/Types/Input/InputMappingContextFile.h"
#include "Engine/Subsystems/Resources/Types/Input/InputMappingContextResource.h"

namespace Opaax::Editor
{
    namespace
    {
        /** One modifier of InType, defaults otherwise. */
        InputModifierData Modifier(const EInputModifier InType)
        {
            InputModifierData lModifier;
            lModifier.Type = InType;
            return lModifier;
        }

        /** Record an action step, unless the gesture changed nothing. */
        bool RecordActionEdit(EditorContext& InContext, const InputActionData& InBefore, const char* InLabel)
        {
            const InputActionData& lAfter = InContext.InputActionDocument.GetData();

            // Compared through the FILE's own serializer rather than field by field: it is the one
            // definition of "different" that cannot drift as fields are added, and it is exactly
            // what IsDirty already asks.
            if (InputActionFile::Serialize(lAfter) == InputActionFile::Serialize(InBefore))
            {
                return false;
            }

            InputActionEdit lStep;
            lStep.ActionPath = InContext.InputActionDocument.AbsPath();
            lStep.Before     = InBefore;
            lStep.After      = lAfter;
            lStep.LabelText  = InLabel;

            InContext.Undo.Record(Move(lStep));
            return true;
        }

        /** Record a mapping-list step, unless nothing changed. */
        bool RecordMappingsEdit(EditorContext& InContext, const TDynArray<InputMappingEntry>& InBefore,
                                const char* InLabel)
        {
            InputMappingsEdit lStep;
            lStep.MapPath   = InContext.InputMapDocument.AbsPath();
            lStep.Before    = InBefore;
            lStep.After     = InContext.InputMapDocument.GetData().Mappings;
            lStep.LabelText = InLabel;

            InContext.Undo.Record(Move(lStep));
            return true;
        }
    }

    // =============================================================================
    // InputActionOps
    // =============================================================================
    bool InputActionOps::CommitEdit(EditorContext& InContext, const InputActionData& InBefore)
    {
        if (!InContext.InputActionDocument.IsOpen()) { return false; }

        return RecordActionEdit(InContext, InBefore, "Edit Input Action");
    }

    bool InputActionOps::AddModifier(EditorContext& InContext)
    {
        if (!InContext.InputActionDocument.IsOpen()) { return false; }

        const InputActionData lBefore = InContext.InputActionDocument.GetData();

        InContext.InputActionDocument.GetMutableData().Modifiers.emplace_back(Modifier(EInputModifier::Scalar));

        return RecordActionEdit(InContext, lBefore, "Add Modifier");
    }

    bool InputActionOps::RemoveModifier(EditorContext& InContext, const Uint32 InIndex)
    {
        if (!InContext.InputActionDocument.IsOpen()) { return false; }

        InputActionData& lData = InContext.InputActionDocument.GetMutableData();
        if (InIndex >= lData.Modifiers.size()) { return false; }

        const InputActionData lBefore = lData;

        lData.Modifiers.erase(lData.Modifiers.begin() + InIndex);

        return RecordActionEdit(InContext, lBefore, "Remove Modifier");
    }

    bool InputActionOps::MoveModifier(EditorContext& InContext, const Uint32 InIndex, const Int32 InDelta)
    {
        if (!InContext.InputActionDocument.IsOpen()) { return false; }

        InputActionData& lData = InContext.InputActionDocument.GetMutableData();
        if (InIndex >= lData.Modifiers.size()) { return false; }

        const Int32 lTarget = static_cast<Int32>(InIndex) + InDelta;
        if (lTarget < 0 || lTarget >= static_cast<Int32>(lData.Modifiers.size())) { return false; }

        const InputActionData lBefore = lData;

        std::swap(lData.Modifiers[InIndex], lData.Modifiers[static_cast<Uint32>(lTarget)]);

        return RecordActionEdit(InContext, lBefore, "Reorder Modifier");
    }

    bool InputActionOps::Save(EditorContext& InContext)
    {
        if (!InContext.InputActionDocument.IsOpen()) { return false; }

        if (!InputActionFile::Save(InContext.InputActionDocument.AbsPath(),
                                   InContext.InputActionDocument.GetData()))
        {
            return false;   // InputActionFile logged why
        }

        InContext.InputActionDocument.MarkSaved();

        InContext.Resources.Reload<InputActionResource>(InContext.InputActionDocument.AbsPath().CStr());

        return true;
    }

    // =============================================================================
    // InputMapOps
    // =============================================================================
    bool InputMapOps::AddMapping(EditorContext& InContext)
    {
        if (!InContext.InputMapDocument.IsOpen()) { return false; }

        const TDynArray<InputMappingEntry> lBefore = InContext.InputMapDocument.GetData().Mappings;

        InContext.InputMapDocument.GetMutableData().Mappings.emplace_back();

        return RecordMappingsEdit(InContext, lBefore, "Add Mapping");
    }

    bool InputMapOps::RemoveMapping(EditorContext& InContext, const Uint32 InIndex)
    {
        if (!InContext.InputMapDocument.IsOpen()) { return false; }

        InputMappingContextData& lData = InContext.InputMapDocument.GetMutableData();
        if (InIndex >= lData.Mappings.size()) { return false; }

        const TDynArray<InputMappingEntry> lBefore = lData.Mappings;

        lData.Mappings.erase(lData.Mappings.begin() + InIndex);

        return RecordMappingsEdit(InContext, lBefore, "Remove Mapping");
    }

    bool InputMapOps::MoveMapping(EditorContext& InContext, const Uint32 InIndex, const Int32 InDelta)
    {
        if (!InContext.InputMapDocument.IsOpen()) { return false; }

        InputMappingContextData& lData = InContext.InputMapDocument.GetMutableData();
        if (InIndex >= lData.Mappings.size()) { return false; }

        const Int32 lTarget = static_cast<Int32>(InIndex) + InDelta;
        if (lTarget < 0 || lTarget >= static_cast<Int32>(lData.Mappings.size())) { return false; }

        const TDynArray<InputMappingEntry> lBefore = lData.Mappings;

        std::swap(lData.Mappings[InIndex], lData.Mappings[static_cast<Uint32>(lTarget)]);

        return RecordMappingsEdit(InContext, lBefore, "Reorder Mapping");
    }

    bool InputMapOps::CommitMappingEdit(EditorContext& InContext, const Uint32 InIndex,
                                        const InputMappingEntry& InBefore)
    {
        if (!InContext.InputMapDocument.IsOpen()) { return false; }

        InputMappingContextData& lData = InContext.InputMapDocument.GetMutableData();
        if (InIndex >= lData.Mappings.size()) { return false; }

        const InputMappingEntry& lAfter = lData.Mappings[InIndex];

        // Nothing to judge beyond "did it change": unlike a mover entry, a mapping has no name to
        // collide and no default to orphan. Two rows binding one key to one action is redundant,
        // not broken — the second is simply consumed by the first.
        if (lAfter.Action.Path == InBefore.Action.Path
            && lAfter.Key == InBefore.Key
            && lAfter.bConsume == InBefore.bConsume
            && lAfter.Modifiers.size() == InBefore.Modifiers.size())
        {
            return false;
        }

        TDynArray<InputMappingEntry> lBeforeList = lData.Mappings;
        lBeforeList[InIndex] = InBefore;

        return RecordMappingsEdit(InContext, lBeforeList, "Edit Mapping");
    }

    bool InputMapOps::AddComposite2D(EditorContext& InContext, const Uint32 InTemplateIndex,
                                     const EKeyCode InUp, const EKeyCode InDown,
                                     const EKeyCode InLeft, const EKeyCode InRight)
    {
        if (!InContext.InputMapDocument.IsOpen()) { return false; }

        InputMappingContextData& lData = InContext.InputMapDocument.GetMutableData();
        if (InTemplateIndex >= lData.Mappings.size()) { return false; }

        const InputMappingEntry lTemplate = lData.Mappings[InTemplateIndex];

        if (lTemplate.Action.IsEmpty())
        {
            OPAAX_LOG(LogEditorInputMapDocument, Warn,
                      "Add 2D composite — the selected mapping names no action, so there is nothing to bind four keys to.");
            return false;
        }

        const TDynArray<InputMappingEntry> lBefore = lData.Mappings;

        // WHICH direction gets which modifier is the only real logic here, so it lives in a pure
        // function this verb calls and a test can reach (see MakeComposite2D).
        const TDynArray<InputMappingEntry> lComposite =
            MakeComposite2D(lTemplate, InUp, InDown, InLeft, InRight);

        // REPLACED, not appended: the template row is what the author dropped the action on, and
        // leaving it beside the four would bind a fifth key nobody asked for.
        lData.Mappings.erase(lData.Mappings.begin() + InTemplateIndex);

        lData.Mappings.insert(lData.Mappings.begin() + InTemplateIndex,
                              lComposite.begin(), lComposite.end());

        return RecordMappingsEdit(InContext, lBefore, "Add 2D Composite");
    }

    bool InputMapOps::AddMappingModifier(EditorContext& InContext, const Uint32 InIndex)
    {
        if (!InContext.InputMapDocument.IsOpen()) { return false; }

        InputMappingContextData& lData = InContext.InputMapDocument.GetMutableData();
        if (InIndex >= lData.Mappings.size()) { return false; }

        const TDynArray<InputMappingEntry> lBefore = lData.Mappings;

        lData.Mappings[InIndex].Modifiers.emplace_back(Modifier(EInputModifier::Scalar));

        return RecordMappingsEdit(InContext, lBefore, "Add Mapping Modifier");
    }

    bool InputMapOps::RemoveMappingModifier(EditorContext& InContext, const Uint32 InIndex,
                                            const Uint32 InModifierIndex)
    {
        if (!InContext.InputMapDocument.IsOpen()) { return false; }

        InputMappingContextData& lData = InContext.InputMapDocument.GetMutableData();
        if (InIndex >= lData.Mappings.size()) { return false; }

        TDynArray<InputModifierData>& lModifiers = lData.Mappings[InIndex].Modifiers;
        if (InModifierIndex >= lModifiers.size()) { return false; }

        const TDynArray<InputMappingEntry> lBefore = lData.Mappings;

        lModifiers.erase(lModifiers.begin() + InModifierIndex);

        return RecordMappingsEdit(InContext, lBefore, "Remove Mapping Modifier");
    }

    bool InputMapOps::SetPriority(EditorContext& InContext, const Int32 InPriority)
    {
        if (!InContext.InputMapDocument.IsOpen()) { return false; }

        InputMappingContextData& lData = InContext.InputMapDocument.GetMutableData();
        if (lData.Priority == InPriority) { return false; }

        InputMapPriorityEdit lStep;
        lStep.MapPath = InContext.InputMapDocument.AbsPath();
        lStep.Before  = lData.Priority;
        lStep.After   = InPriority;

        lData.Priority = InPriority;

        InContext.Undo.Record(Move(lStep));
        return true;
    }

    bool InputMapOps::Save(EditorContext& InContext)
    {
        if (!InContext.InputMapDocument.IsOpen()) { return false; }

        if (!InputMappingContextFile::Save(InContext.InputMapDocument.AbsPath(),
                                           InContext.InputMapDocument.GetData()))
        {
            return false;   // InputMappingContextFile logged why
        }

        InContext.InputMapDocument.MarkSaved();

        InContext.Resources.Reload<InputMappingContextResource>(InContext.InputMapDocument.AbsPath().CStr());

        return true;
    }
}
