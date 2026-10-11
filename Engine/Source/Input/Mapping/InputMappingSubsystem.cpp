#include "Input/Mapping/InputMappingSubsystem.h"

#include "Application/Services/IPaths.h"
#include "Engine/GameInstance/GameInstanceContext.h"
#include "Input/InputKeyNames.h"   // ToString(EKeyCode)
#include "Resources/ResourceManager.h"
#include "Input/Assets/InputActionResource.h"
#include "Input/Assets/InputMappingContextResource.h"
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

            // A missing file gives a placeholder action without a name.
            if (lAction == nullptr || !lAction->Data.Name.IsValid())
            {
                OPAAX_LOG(LogInputMapping, Warn,
                          "Context '{}' — action '{}' did not load or has no name. Skipped.",
                          InName, lEntry.Action.Path.CStr());
                continue;
            }

            // Registered on first use, so a context needs no prior action declaration.
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

        // Everything needed was copied, so the assets may be unloaded.
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

        // Allowed (the context may be added later), but warn: it may be a typo.
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
        // Check: the game must start before any Play world exists.
        OPAAX_LOG(LogInputMapping, Trace,
                  "Input mapping started before any play world exists ({} play world(s), {} total) — {} action(s), {} context(s)",
                  m_Context->Worlds.CountWorldsOfMode(EWorldMode::Play),
                  m_Context->Worlds.GetWorldCount(),
                  m_Evaluator.GetActionCount(), m_Evaluator.GetContextCount());
        return true;
    }

    void InputMappingSubsystem::ConsumeThisFrame(const InputKeyMask& InConsumed)
    {
        for (Uint16 lIndex = 0; lIndex < InputManager::KEY_STATE_COUNT; ++lIndex)
        {
            if (InConsumed[lIndex]) { m_PreConsumed[lIndex] = true; }
        }
    }

    void InputMappingSubsystem::Update(double InDeltaTime)
    {
        m_Evaluator.Evaluate(m_Context->Input, InDeltaTime, &m_PreConsumed);
        m_PreConsumed = InputKeyMask{};   // this frame only

        // By index, re-read each step: a handler may Bind or RegisterAction during its callback.
        for (Uint64 lIdx = 0; lIdx < m_Bindings.size(); ++lIdx)
        {
            const OpaaxStringID lAction = m_Bindings[lIdx].Action;

            const InputActionState* lFound = m_Evaluator.FindState(lAction);
            if (lFound == nullptr)
            {
                continue;
            }

            // Copied, so registering an action cannot leave it dangling.
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
    }
}
