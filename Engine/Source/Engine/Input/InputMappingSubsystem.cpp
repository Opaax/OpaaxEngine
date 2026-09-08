#include "Engine/Input/InputMappingSubsystem.h"

#include "Application/Services/IPaths.h"
#include "Engine/GameInstance/GameInstanceContext.h"
#include "Engine/Subsystems/Input/InputKeyNames.h"   // ToString(EKeyCode) for the skip warnings
#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Engine/Subsystems/Resources/Types/Input/InputActionResource.h"
#include "Engine/Subsystems/Resources/Types/Input/InputMappingContextResource.h"
#include "World/WorldManager.h"

namespace Opaax
{
    InputMappingSubsystem::InputMappingSubsystem(GameInstanceContext& InContext) noexcept
        : m_Context(&InContext)
    {
    }

    // =========================================================================
    // Actions and contexts
    // =========================================================================
    bool InputMappingSubsystem::RegisterAction(const InputAction& InAction)
    {
        return m_Evaluator.RegisterAction(InAction);
    }

    bool InputMappingSubsystem::AddContext(const InputMappingContext& InContext)
    {
        return m_Evaluator.AddContext(InContext);
    }

    bool InputMappingSubsystem::AddContextAsset(OpaaxStringID InName, const OpaaxString& InAssetPath)
    {
        const OpaaxString lAbsolute = m_Context->Paths.AssetToAbsolute(InAssetPath);

        const ResourceRef<InputMappingContextResource> lMapRef =
            m_Context->Resources.Load<InputMappingContextResource>(lAbsolute.CStr());

        const InputMappingContextResource* lMap = lMapRef.Get();
        if (lMap == nullptr)
        {
            OPAAX_LOG(LogInputMapping, Error, "AddContextAsset '{}' — '{}' did not load.",
                      InName, InAssetPath.CStr());
            return false;
        }

        InputMappingContext lContext;
        lContext.Name     = InName;
        lContext.Priority = lMap->Data.Priority;

        for (const InputMappingEntry& lEntry : lMap->Data.Mappings)
        {
            if (lEntry.Action.IsEmpty())
            {
                OPAAX_LOG(LogInputMapping, Warn, "Context '{}' — a mapping for key '{}' names no action. Skipped.",
                          InName, ToString(lEntry.Key));
                continue;
            }

            const OpaaxString lActionAbs = m_Context->Paths.AssetToAbsolute(lEntry.Action.Path);

            const ResourceRef<InputActionResource> lActionRef =
                m_Context->Resources.Load<InputActionResource>(lActionAbs.CStr());

            const InputActionResource* lAction = lActionRef.Get();

            // A Placeholder action has no name, which is exactly how a missing file shows up here.
            if (lAction == nullptr || !lAction->Data.Name.IsValid())
            {
                OPAAX_LOG(LogInputMapping, Warn,
                          "Context '{}' — action '{}' did not load or has no name. Skipped.",
                          InName, lEntry.Action.Path.CStr());
                continue;
            }

            // Registered on FIRST SIGHT, so a context is self-sufficient — nothing has to declare
            // the actions before the map that uses them. A second context naming the same action
            // finds it already there and reuses the same definition.
            if (m_Evaluator.FindAction(lAction->Data.Name) == nullptr)
            {
                InputAction lDefinition;
                lDefinition.Name        = lAction->Data.Name;
                lDefinition.ValueType   = lAction->Data.ValueType;
                lDefinition.HoldSeconds = lAction->Data.HoldSeconds;
                lDefinition.Modifiers   = lAction->Data.Modifiers;

                m_Evaluator.RegisterAction(lDefinition);
            }

            InputKeyBinding lBinding;
            lBinding.Action    = lAction->Data.Name;
            lBinding.Key       = lEntry.Key;
            lBinding.Modifiers = lEntry.Modifiers;
            lBinding.bConsume  = lEntry.bConsume;

            lContext.Bindings.emplace_back(Move(lBinding));
        }

        // The refs go out of scope here on purpose: everything the evaluator needs — the name, the
        // key, the modifier list — was COPIED above, so nothing downstream depends on the assets
        // staying resident.
        return m_Evaluator.AddContext(lContext);
    }

    bool InputMappingSubsystem::RemoveContext(OpaaxStringID InName)
    {
        return m_Evaluator.RemoveContext(InName);
    }

    // =========================================================================
    // Binding
    // =========================================================================
    FOnInputAction& InputMappingSubsystem::DelegateFor(OpaaxStringID InAction, EInputTrigger InTrigger)
    {
        for (ActionDelegates& lEntry : m_Bindings)
        {
            if (lEntry.Action == InAction)
            {
                return lEntry.ByTrigger[static_cast<Uint8>(InTrigger)];
            }
        }

        m_Bindings.emplace_back();
        m_Bindings.back().Action = InAction;

        return m_Bindings.back().ByTrigger[static_cast<Uint8>(InTrigger)];
    }

    void InputMappingSubsystem::WarnIfUnknownAction(OpaaxStringID InAction, EInputTrigger InTrigger) const
    {
        if (m_Evaluator.FindAction(InAction) != nullptr)
        {
            return;
        }

        // Not refused: a caller may legitimately bind before the context that declares the action
        // is added. But a typo is silent forever otherwise, which is the risk proposal 04 named.
        OPAAX_LOG(LogInputMapping, Warn,
                  "Bind '{}' ({}) — no action of that name is registered yet. It will never fire unless one is added.",
                  InAction, ToString(InTrigger));
    }

    DelegateHandle InputMappingSubsystem::Bind(OpaaxStringID InAction, EInputTrigger InTrigger,
                                              TFunction<void(const InputActionValue&)> InCallback)
    {
        WarnIfUnknownAction(InAction, InTrigger);
        return DelegateFor(InAction, InTrigger).Add(Move(InCallback));
    }

    bool InputMappingSubsystem::Unbind(OpaaxStringID InAction, EInputTrigger InTrigger, DelegateHandle InHandle)
    {
        for (ActionDelegates& lEntry : m_Bindings)
        {
            if (lEntry.Action == InAction)
            {
                return lEntry.ByTrigger[static_cast<Uint8>(InTrigger)].Remove(InHandle);
            }
        }

        return false;
    }

    Uint64 InputMappingSubsystem::UnbindAll(void* InOwner)
    {
        const Uint64 lBefore = GetBindingCount();

        for (ActionDelegates& lEntry : m_Bindings)
        {
            for (FOnInputAction& lDelegate : lEntry.ByTrigger)
            {
                lDelegate.RemoveAll(InOwner);
            }
        }

        return lBefore - GetBindingCount();
    }

    Uint64 InputMappingSubsystem::GetBindingCount() const noexcept
    {
        Uint64 lCount = 0;

        for (const ActionDelegates& lEntry : m_Bindings)
        {
            for (const FOnInputAction& lDelegate : lEntry.ByTrigger)
            {
                lCount += lDelegate.Num();
            }
        }

        return lCount;
    }

    // =========================================================================
    // Query
    // =========================================================================
    InputActionValue InputMappingSubsystem::GetValue(OpaaxStringID InAction) const noexcept
    {
        return m_Evaluator.GetValue(InAction);
    }

    const InputActionState* InputMappingSubsystem::FindState(OpaaxStringID InAction) const noexcept
    {
        return m_Evaluator.FindState(InAction);
    }

    // =========================================================================
    // Lifecycle
    // =========================================================================
    bool InputMappingSubsystem::Startup()
    {
        // The PLAY world count is the GATE, not decoration: a game is started before any world
        // that belongs to it, so anything other than 0 here means the session was created in
        // reaction to a world and every WorldContext built before this point missed it.
        //
        // PLAY worlds specifically, not all of them — the editor starts a game while its EDIT
        // world is on screen, so a total count reads 1 there and cannot tell a correct boot from
        // a broken one. Total is printed alongside only as context.
        OPAAX_LOG(LogInputMapping, Info,
                  "Input mapping started before any play world exists ({} play world(s), {} total) — {} action(s), {} context(s)",
                  m_Context->Worlds.CountWorldsOfMode(EWorldMode::Play),
                  m_Context->Worlds.GetWorldCount(),
                  m_Evaluator.GetActionCount(), m_Evaluator.GetContextCount());
        return true;
    }

    void InputMappingSubsystem::Update(double InDeltaTime)
    {
        m_Evaluator.Evaluate(m_Context->Input, InDeltaTime);

        // BY INDEX and re-read each step: a handler may Bind (growing m_Bindings and
        // reallocating it) or RegisterAction (invalidating a state pointer) from inside its own
        // callback. Broadcast itself iterates a snapshot, so adding a listener mid-dispatch is
        // already safe — this is about the two containers around it.
        for (Uint64 lIdx = 0; lIdx < m_Bindings.size(); ++lIdx)
        {
            const OpaaxStringID lAction = m_Bindings[lIdx].Action;

            const InputActionState* lFound = m_Evaluator.FindState(lAction);
            if (lFound == nullptr)
            {
                continue;
            }

            // COPIED, so a handler that registers an action cannot leave this dangling.
            const InputActionState lState = *lFound;

            for (Uint8 lTrigger = 0; lTrigger < INPUT_TRIGGER_COUNT; ++lTrigger)
            {
                if (!lState.FiresFor(static_cast<EInputTrigger>(lTrigger)))
                {
                    continue;
                }

                if (lIdx >= m_Bindings.size())
                {
                    break;
                }

                m_Bindings[lIdx].ByTrigger[lTrigger].Broadcast(lState.Value);
            }
        }
    }

    void InputMappingSubsystem::Shutdown()
    {
        OPAAX_LOG(LogInputMapping, Info,
                  "Input mapping shutdown ({} action(s), {} context(s), {} binding(s) still registered)",
                  m_Evaluator.GetActionCount(), m_Evaluator.GetContextCount(), GetBindingCount());
    }
}
