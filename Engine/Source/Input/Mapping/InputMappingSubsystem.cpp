#include "Input/Mapping/InputMappingSubsystem.h"

#include "Application/Services/IPaths.h"
#include "Application/Services/IUserSettings.h"
#include "Core/Algo/Algo.h"
#include "Engine/GameInstance/GameInstanceContext.h"
#include "Input/Assets/InputActionAsset.h"
#include "Input/Assets/InputMappingContextAsset.h"
#include "Input/InputKeyNames.h"   // ToString(EKeyCode)
#include "Resources/Asset/AssetHandle.h"
#include "Resources/Asset/AssetMigration.h"
#include "Resources/ResourceManager.h"
#include "World/WorldManager.h"

namespace Opaax
{
    InputMappingSubsystem::InputMappingSubsystem()  = default;
    InputMappingSubsystem::~InputMappingSubsystem() = default;

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
        const KnownContext* lKnown = LoadKnown(InAssetPath);
        if (lKnown == nullptr)
        {
            OPAAX_LOG(LogInputMapping, Error, "AddContextAsset '{}' — '{}' did not load.",
                      InName, InAssetPath.CStr());
            return false;
        }

        InputMappingContext lContext;
        lContext.Name     = InName;
        lContext.Priority = lKnown->Priority;
        lContext.Bindings = BuildBindings(*lKnown);

        if (!m_Evaluator.AddContext(lContext))
        {
            return false;
        }

        const OpaaxString lPath = lKnown->Layout.Context;
        if (!Algo::AnyOf(m_Added, [InName](const AddedContext& InAdded) { return InAdded.Name == InName; }))
        {
            m_Added.push_back(AddedContext{ InName, lPath });
        }
        return true;
    }

    bool InputMappingSubsystem::RemoveContext(OpaaxStringID InName)
    {
        Algo::RemoveIf(m_Added, [InName](const AddedContext& InAdded) { return InAdded.Name == InName; });
        return m_Evaluator.RemoveContext(InName);
    }

    // =========================================================================
    // The player's keys
    // =========================================================================
    bool InputMappingSubsystem::RegisterContextAsset(const OpaaxString& InAssetPath)
    {
        if (LoadKnown(InAssetPath) == nullptr)
        {
            OPAAX_LOG(LogInputMapping, Error, "RegisterContextAsset — '{}' did not load.", InAssetPath.CStr());
            return false;
        }
        return true;
    }

    TDynArray<InputKeyMapping> InputMappingSubsystem::GetKeyMappings(const OpaaxString& InAssetPath) const
    {
        TDynArray<InputKeyMapping> lMappings;

        const KnownContext* lKnown = FindKnown(InAssetPath);
        if (lKnown == nullptr)
        {
            return lMappings;
        }

        lMappings.reserve(lKnown->Layout.Slots.size());
        for (const InputKeySlot& lSlot : lKnown->Layout.Slots)
        {
            lMappings.push_back(InputKeyMapping{ lSlot, m_Rebinds.GetKey(lSlot) });
        }
        return lMappings;
    }

    EKeyCode InputMappingSubsystem::GetKey(const InputKeySlot& InSlot) const
    {
        return m_Rebinds.GetKey(CanonicalSlot(InSlot));
    }

    InputRebindResult InputMappingSubsystem::Rebind(const InputKeySlot& InSlot, const EKeyCode InKey,
                                                    const EKeyConflictPolicy InPolicy)
    {
        const InputKeySlot lSlot   = CanonicalSlot(InSlot);
        const EKeyCode     lBefore = m_Rebinds.GetKey(lSlot);

        const InputRebindResult lResult = m_Rebinds.Rebind(GetLayouts(), lSlot, InKey, InPolicy, m_KeyCheck);
        if (!lResult.IsOk())
        {
            OPAAX_LOG(LogInputMapping, Info, "Rebind {} {} on {} to {}: {}{}{}", lSlot.Context.CStr(), lSlot.Action,
                      ToString(lSlot.DefaultKey), ToString(InKey), ToString(lResult.Result),
                      lResult.Why.IsEmpty() ? "" : ", ", lResult.Why.CStr());
            return lResult;
        }

        if (m_Rebinds.GetKey(lSlot) != lBefore)
        {
            OPAAX_LOG(LogInputMapping, Info, "Rebound {} {} on {}: {} (was {}){}", lSlot.Context.CStr(), lSlot.Action,
                      ToString(lSlot.DefaultKey), ToString(InKey), ToString(lBefore),
                      lResult.Others.empty() ? "" : OpaaxString::Format(", {} other(s) {}", lResult.Others.size(),
                          InPolicy == EKeyConflictPolicy::Swap ? "swapped" : "cleared").CStr());
            KeysChanged();
        }
        return lResult;
    }

    void InputMappingSubsystem::ResetKey(const InputKeySlot& InSlot)
    {
        const InputKeySlot lSlot = CanonicalSlot(InSlot);
        if (m_Rebinds.GetKey(lSlot) == lSlot.DefaultKey)
        {
            return;
        }
        m_Rebinds.Reset(lSlot);
        KeysChanged();
    }

    void InputMappingSubsystem::ResetKeys(const OpaaxString& InAssetPath)
    {
        const OpaaxString lPath = CanonicalPath(InAssetPath);
        if (!m_Rebinds.HasRebinds(lPath))
        {
            return;
        }
        m_Rebinds.ResetContext(lPath);
        KeysChanged();
    }

    void InputMappingSubsystem::ResetAllKeys()
    {
        if (m_Rebinds.IsEmpty())
        {
            return;
        }
        m_Rebinds.ResetAll();
        KeysChanged();
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
        // The player's keys, from the settings the game started with; read again when they change there.
        // Without settings (the null ones) the keys are kept for this game only.
        IUserSettings& lSettings = GetContext().Settings;
        if (!lSettings.IsNull())
        {
            m_Settings = &lSettings;
            m_Settings->OnSectionChanged().AddMember(this, &InputMappingSubsystem::HandleSettingsSectionChanged);
        }
        ReadKeys();

        // Check: the game must start before any Play world exists.
        OPAAX_LOG(LogInputMapping, Trace,
                  "Input mapping started before any play world exists ({} play world(s), {} total) — {} action(s), {} context(s)",
                  GetContext().Worlds.CountWorldsOfMode(EWorldMode::Play),
                  GetContext().Worlds.GetWorldCount(),
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
        // A capture takes its key before the actions read theirs: while it waits every key is masked,
        // then the key it took until released.
        UpdateKeyCapture();
        if (m_bCapturing)
        {
            for (bool& lMasked : m_PreConsumed) { lMasked = true; }
        }
        else if (m_CapturedHeld != EKeyCode::None)
        {
            m_PreConsumed[static_cast<Uint16>(m_CapturedHeld)] = true;
        }

        m_Evaluator.Evaluate(GetContext().Input, InDeltaTime, &m_PreConsumed);
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
        // Nobody is told: the game is ending.
        m_bCapturing   = false;
        m_CapturedHeld = EKeyCode::None;

        if (m_Settings != nullptr)
        {
            m_Settings->OnSectionChanged().RemoveAll(this);
            m_Settings = nullptr;
        }
    }

    // =========================================================================
    // Waiting for a key
    // =========================================================================
    void InputMappingSubsystem::BeginKeyCapture()
    {
        const InputManager& lInput = GetContext().Input;

        // What is down now (the click that asked) counts once released and pressed again.
        for (Uint16 lIndex = 0; lIndex < InputManager::KEY_STATE_COUNT; ++lIndex)
        {
            const EKeyCode lKey      = static_cast<EKeyCode>(lIndex);
            m_CaptureIgnored[lIndex] = lInput.IsKeyDown(lKey) || lInput.WasPressedThisFrame(lKey);
        }

        m_bCapturing = true;
        OPAAX_LOG(LogInputMapping, Trace, "Key capture: waiting for a key (Escape cancels)");
    }

    void InputMappingSubsystem::CancelKeyCapture()
    {
        if (m_bCapturing)
        {
            EndKeyCapture(EKeyCode::None);
        }
    }

    void InputMappingSubsystem::UpdateKeyCapture()
    {
        const InputManager& lInput = GetContext().Input;

        const auto bUp = [&lInput](const EKeyCode InKey)
        {
            return !lInput.IsKeyDown(InKey) && !lInput.WasPressedThisFrame(InKey);
        };

        if (m_CapturedHeld != EKeyCode::None && bUp(m_CapturedHeld))
        {
            m_CapturedHeld = EKeyCode::None;
        }

        if (!m_bCapturing)
        {
            return;
        }

        for (Uint16 lIndex = 0; lIndex < InputManager::KEY_STATE_COUNT; ++lIndex)
        {
            if (m_CaptureIgnored[lIndex] && bUp(static_cast<EKeyCode>(lIndex)))
            {
                m_CaptureIgnored[lIndex] = false;
            }
        }

        const auto bNewPress = [this, &lInput](const EKeyCode InKey)
        {
            return lInput.WasPressedThisFrame(InKey) && !m_CaptureIgnored[static_cast<Uint16>(InKey)];
        };

        // The Escape that cancels is held as a key taken: it does not close the menu too.
        if (bNewPress(EKeyCode::Escape))
        {
            m_CapturedHeld = EKeyCode::Escape;
            EndKeyCapture(EKeyCode::None);
            return;
        }

        // Several in one frame: the lowest code, so a run gives the same key every time.
        for (Uint16 lIndex = 0; lIndex < InputManager::KEY_STATE_COUNT; ++lIndex)
        {
            const EKeyCode lKey = static_cast<EKeyCode>(lIndex);
            if (IsKeyCodeBindable(lKey) && lKey != EKeyCode::Mouse_WheelAxis && bNewPress(lKey))
            {
                m_CapturedHeld = lKey;
                EndKeyCapture(lKey);
                return;
            }
        }
    }

    void InputMappingSubsystem::EndKeyCapture(const EKeyCode InKey)
    {
        m_bCapturing     = false;
        m_CaptureIgnored = InputKeyMask{};

        OPAAX_LOG(LogInputMapping, Info, "Key capture: {}", InKey == EKeyCode::None ? "cancelled" : ToString(InKey));

        // Last: a listener may rebind, or start another capture.
        m_OnKeyCaptured.Broadcast(InKey);
    }

    // =========================================================================
    // The player's keys — internal
    // =========================================================================
    const InputMappingSubsystem::KnownContext* InputMappingSubsystem::LoadKnown(const OpaaxString& InAssetPath)
    {
        const TAssetHandle<InputMappingContextAsset> lMapHandle =
            LoadAsset<InputMappingContextAsset>(GetContext().Resources, GetContext().Paths, InAssetPath);

        const InputMappingContextAsset* lMap = lMapHandle.Get();
        if (lMap == nullptr)
        {
            return nullptr;
        }

        KnownContext lKnown;
        lKnown.Layout.Context     = lMapHandle.GetPath();
        lKnown.Layout.bRebindable = lMap->bRebindable;
        lKnown.Priority           = lMap->Priority;

        for (const InputMappingEntry& lEntry : lMap->Mappings)
        {
            if (lEntry.Action.IsEmpty())
            {
                OPAAX_LOG(LogInputMapping, Warn, "Context '{}' — a mapping for key '{}' names no action. Skipped.",
                          lKnown.Layout.Context.CStr(), ToString(lEntry.Key));
                continue;
            }

            const TAssetHandle<InputActionAsset> lActionHandle =
                LoadAsset(GetContext().Resources, GetContext().Paths, lEntry.Action);

            const InputActionAsset* lAction = lActionHandle.Get();
            if (lAction == nullptr)
            {
                OPAAX_LOG(LogInputMapping, Warn, "Context '{}' — action '{}' did not load. Skipped.",
                          lKnown.Layout.Context.CStr(), lEntry.Action.Path.CStr());
                continue;
            }

            // Registered on first use, so a context needs no prior action declaration.
            const OpaaxStringID lActionName = lAction->GetActionName();
            if (m_Evaluator.FindAction(lActionName) == nullptr)
            {
                m_Evaluator.RegisterAction(lAction->ToAction());
            }

            InputKeyBinding lBinding;
            lBinding.Action    = lActionName;
            lBinding.Key       = lEntry.Key;
            lBinding.Modifiers = lEntry.Modifiers;
            lBinding.bConsume  = lEntry.bConsume;

            lKnown.Layout.Slots.push_back(InputKeySlot{ lKnown.Layout.Context, lActionName, lEntry.Key });
            lKnown.Bindings.emplace_back(Move(lBinding));
        }

        // Everything needed was copied, so the assets may be unloaded. Read again, it replaces what was known.
        KnownContext* lKept = nullptr;
        for (KnownContext& lExisting : m_Known)
        {
            if (lExisting.Layout.Context == lKnown.Layout.Context)
            {
                lExisting = Move(lKnown);
                lKept     = &lExisting;
                break;
            }
        }
        if (lKept == nullptr)
        {
            m_Known.emplace_back(Move(lKnown));
            lKept = &m_Known.back();
        }

        // Keys kept for a mapping the asset no longer has are dropped, and the settings told.
        if (DropUnknownKeys(*lKept))
        {
            KeysChanged();
        }
        return lKept;
    }

    const InputMappingSubsystem::KnownContext* InputMappingSubsystem::FindKnown(const OpaaxString& InAssetPath) const
    {
        const OpaaxString lPath = CanonicalPath(InAssetPath);
        for (const KnownContext& lKnown : m_Known)
        {
            if (lKnown.Layout.Context == lPath)
            {
                return &lKnown;
            }
        }
        return nullptr;
    }

    OpaaxString InputMappingSubsystem::CanonicalPath(const OpaaxString& InAssetPath)
    {
        OpaaxString lConverted = AssetMigration::ConvertedPath(InAssetPath);
        return lConverted.IsEmpty() ? InAssetPath : lConverted;
    }

    InputKeySlot InputMappingSubsystem::CanonicalSlot(const InputKeySlot& InSlot)
    {
        InputKeySlot lSlot = InSlot;
        lSlot.Context      = CanonicalPath(InSlot.Context);
        return lSlot;
    }

    TDynArray<InputContextLayout> InputMappingSubsystem::GetLayouts() const
    {
        TDynArray<InputContextLayout> lLayouts;
        lLayouts.reserve(m_Known.size());
        for (const KnownContext& lKnown : m_Known)
        {
            lLayouts.push_back(lKnown.Layout);
        }
        return lLayouts;
    }

    TDynArray<InputKeyBinding> InputMappingSubsystem::BuildBindings(const KnownContext& InKnown) const
    {
        TDynArray<InputKeyBinding> lBindings;
        lBindings.reserve(InKnown.Bindings.size());

        for (Uint64 lIndex = 0; lIndex < InKnown.Bindings.size(); ++lIndex)
        {
            InputKeyBinding lBinding = InKnown.Bindings[lIndex];
            lBinding.Key             = m_Rebinds.GetKey(InKnown.Layout.Slots[lIndex]);
            if (lBinding.Key != EKeyCode::None)
            {
                lBindings.emplace_back(Move(lBinding));
            }
        }
        return lBindings;
    }

    void InputMappingSubsystem::ApplyKeys()
    {
        for (const AddedContext& lAdded : m_Added)
        {
            if (const KnownContext* lKnown = FindKnown(lAdded.AssetPath))
            {
                m_Evaluator.SetContextBindings(lAdded.Name, BuildBindings(*lKnown));
            }
        }
    }

    void InputMappingSubsystem::KeysChanged()
    {
        ApplyKeys();

        if (m_Settings != nullptr)
        {
            m_bWritingKeys = true;
            m_Settings->SetSection(KEYS_SECTION, m_Rebinds.ToJson());
            m_bWritingKeys = false;
        }

        m_OnKeysChanged.Broadcast();
    }

    void InputMappingSubsystem::ReadKeys()
    {
        TDynArray<OpaaxString> lDropped;
        m_Rebinds.FromJson(m_Settings != nullptr ? m_Settings->GetSection(KEYS_SECTION) : nlohmann::json(nullptr),
                           &lDropped);
        for (const OpaaxString& lWhy : lDropped)
        {
            OPAAX_LOG(LogInputMapping, Warn, "The player's keys: {}. Dropped.", lWhy.CStr());
        }

        bool bDroppedKnown = false;
        for (const KnownContext& lKnown : m_Known)
        {
            bDroppedKnown = DropUnknownKeys(lKnown) || bDroppedKnown;
        }

        if (!m_Rebinds.IsEmpty())
        {
            OPAAX_LOG(LogInputMapping, Info, "The player's keys: {} rebound", m_Rebinds.Num());
        }

        // Written back only when some were dropped; applied and told either way.
        if (bDroppedKnown || !lDropped.empty())
        {
            KeysChanged();
        }
        else
        {
            ApplyKeys();
            m_OnKeysChanged.Broadcast();
        }
    }

    bool InputMappingSubsystem::DropUnknownKeys(const KnownContext& InKnown)
    {
        TDynArray<OpaaxString> lDropped;
        m_Rebinds.DropUnknown(InKnown.Layout, &lDropped);
        for (const OpaaxString& lWhy : lDropped)
        {
            OPAAX_LOG(LogInputMapping, Warn, "The player's keys: {}. Dropped.", lWhy.CStr());
        }
        return !lDropped.empty();
    }

    void InputMappingSubsystem::HandleSettingsSectionChanged(const OpaaxStringView InSection)
    {
        if (!m_bWritingKeys && InSection == KEYS_SECTION)
        {
            ReadKeys();
        }
    }
}
