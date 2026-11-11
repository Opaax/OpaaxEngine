#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Log/Logger.h"
#include "Core/Events/Delegate.h"
#include "Engine/GameInstance/GameInstanceSubsystem.h"
#include "Core/String/OpaaxStringView.hpp"
#include "Input/Mapping/InputActionEvaluator.h"
#include "Input/Mapping/InputRebinding.h"
#include "Input/Mapping/InputTypes.h"

namespace Opaax
{

    inline constexpr LogCategory LogInputMapping{"InputMapping"};

    class IUserSettings;

    /** Passed to a bound handler: the action's value when it fired. */
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnInputAction, const InputActionValue&)

    /** The player's keys changed: a rebind, a reset, the settings read again. A controls menu redraws. */
    DECLARE_MULTICAST_DELEGATE(FOnInputKeysChanged)

    /** Passed the key a capture took (BeginKeyCapture); EKeyCode::None when it was cancelled. */
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnKeyCaptured, EKeyCode)

    // =============================================================================
    // InputMappingSubsystem — turns raw keys (InputManager) into named actions.
    //   Game-instance subsystem, so mapping contexts survive level changes.
    //   Updated before the worlds, so action values are ready for gameplay.
    //
    //   Gameplay binds handlers with Bind(action, trigger, this, &T::Handler).
    //   The owner must call UnbindAll(this) in its Shutdown.
    //
    //   The player rebinds the keys of context assets (Rebind): by the context's asset, the action and
    //   the key the asset gives it, whether the context is added or not. The keys are kept in the
    //   player's settings (IUserSettings, section KEYS_SECTION) and saved with them.
    // =============================================================================
    class InputMappingSubsystem final : public GameInstanceSubsystem
    {
        OPAAX_OBJECT(InputMappingSubsystem, GameInstanceSubsystem)

        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        InputMappingSubsystem();
        ~InputMappingSubsystem() override;

        /** The player's settings' section the keys are kept in. */
        static constexpr const char* KEYS_SECTION = "Keys";

        // =========================================================================
        // Actions and contexts
        // =========================================================================
    public:
        /** Declares an action. Refused for an empty or already used name. */
        bool RegisterAction(const InputAction& InAction);

        /**
         * Adds a mapping context. Higher priority is evaluated (and consumes keys) first.
         * Bindings to unknown actions or gamepad codes are skipped with a warning.
         */
        bool AddContext(const InputMappingContext& InContext);

        /**
         * Loads an input mapping context asset, registers the actions it uses, and adds it as a context.
         * @param InName      Name to use with RemoveContext
         * @param InAssetPath Asset-relative path ("Input/Gameplay.opaaxasset")
         */
        bool AddContextAsset(OpaaxStringID InName, const OpaaxString& InAssetPath);

        /** @return True if a context with that name was removed */
        bool RemoveContext(OpaaxStringID InName);

        // =========================================================================
        // The player's keys
        // =========================================================================
    public:
        /**
         * Reads a context asset without adding it: a controls menu lists its keys and rebinds them, and
         * a locked one's keys are reserved, before the context is used. An added asset is known too.
         * @return False if it did not load
         */
        bool RegisterContextAsset(const OpaaxString& InAssetPath);

        /** Every known context asset (added or registered), in the order read: what a controls menu may list. */
        TDynArray<InputContextLayout> GetContextLayouts() const;

        /** The layout of a known context asset (an old format's path too), or null; valid until another asset is read. */
        const InputContextLayout* FindContextLayout(const OpaaxString& InAssetPath) const;

        /** True while a context added from the asset is in use (AddContextAsset, not removed since). */
        bool IsContextAssetAdded(const OpaaxString& InAssetPath) const;

        /** The mappings of a known context asset with the player's keys, in the asset's order; none if unknown. */
        TDynArray<InputKeyMapping> GetKeyMappings(const OpaaxString& InAssetPath) const;

        /** The key InSlot has: the player's, else the asset's. */
        EKeyCode GetKey(const InputKeySlot& InSlot) const;

        /**
         * Gives InSlot the key InKey (EKeyCode::None clears it), at once in every context added from its
         * asset. Another mapping of the context on the key is a conflict handled by InPolicy; a locked
         * context's key is never taken; the game's key check (SetKeyCheck) has the last say. Kept in the
         * player's settings, not saved (IUserSettings::Save).
         */
        InputRebindResult Rebind(const InputKeySlot& InSlot, EKeyCode InKey,
                                 EKeyConflictPolicy InPolicy = EKeyConflictPolicy::Refuse);

        /** InSlot gets the asset's key again. */
        void ResetKey(const InputKeySlot& InSlot);

        /** Every mapping of the context asset gets the asset's key again. */
        void ResetKeys(const OpaaxString& InAssetPath);

        /** Every mapping gets its asset's key again. */
        void ResetAllKeys();

        /** True when the player rebound a key, of any context. */
        bool HasRebinds() const noexcept { return !m_Rebinds.IsEmpty(); }

        /**
         * The game's say on every key a rebind gives (one check a game; set again to replace it, empty to
         * remove it): false refuses the rebind, with the reason for the menu.
         */
        void SetKeyCheck(FInputKeyCheck InCheck) { m_KeyCheck = Move(InCheck); }

        /** Fires after the player's keys changed. */
        FOnInputKeysChanged& OnKeysChanged() noexcept { return m_OnKeysChanged; }

        // =========================================================================
        // Waiting for a key
        // =========================================================================
    public:
        /**
         * Waits for the next key or mouse button the player presses: a controls menu's "press a key".
         * A key held when it starts counts once released and pressed again, so the click or the Enter
         * that started it is not taken. Escape cancels. While it waits no key drives an action and the
         * UI gets no input; the key it takes, or the Escape that cancels, drives nothing until released.
         * OnKeyCaptured tells the key. A capture under way starts over.
         */
        void BeginKeyCapture();

        /** Ends a capture under way: OnKeyCaptured with EKeyCode::None. */
        void CancelKeyCapture();

        bool IsCapturingKey() const noexcept { return m_bCapturing; }

        /** The key the last capture to end took: EKeyCode::None when it was cancelled; empty before any ended. */
        TOptional<EKeyCode> GetLastCapturedKey() const noexcept { return m_LastCaptured; }

        /**
         * True while a capture waits or the key it took is still held, while a tool holds the input
         * (HoldInput), or a key down when it let go is still held: the UI routes nothing (UISubsystem).
         */
        bool IsHoldingInput() const noexcept
        {
            return m_bCapturing || m_CapturedHeld != EKeyCode::None || m_Holds > 0 || m_bReleaseHeld;
        }

        /** Fires when a capture ends; an owner removes itself in its Shutdown (RemoveAll(this)), as with Bind. */
        FOnKeyCaptured& OnKeyCaptured() noexcept { return m_OnKeyCaptured; }

        // =========================================================================
        // Holding the input
        // =========================================================================
    public:
        /**
         * A tool drawn over the game (the dev console) takes the keys: no key drives an action and the UI
         * gets no input until ReleaseInput. Each HoldInput needs its ReleaseInput.
         */
        void HoldInput() noexcept { ++m_Holds; }

        /**
         * Ends one HoldInput. At the last, the keys still down (the Escape that closed the tool) drive
         * nothing until released, as the key a capture took.
         */
        void ReleaseInput();

        /** True between a HoldInput and its ReleaseInput. */
        bool IsInputHeld() const noexcept { return m_Holds > 0; }

        // =========================================================================
        // Binding
        // =========================================================================
    public:
        /**
         * Calls InMember on InOwner when InAction fires for InTrigger.
         * @return Handle for Unbind (optional: UnbindAll(this) needs none)
         */
        template<typename T>
        DelegateHandle Bind(OpaaxStringID InAction, EInputTrigger InTrigger, T* InOwner,
                            void (T::*InMember)(const InputActionValue&))
        {
            WarnIfUnknownAction(InAction, InTrigger);
            return DelegateFor(InAction, InTrigger).AddMember(InOwner, InMember);
        }

        /** Lambda version. No owner, so it can only be removed by handle. */
        DelegateHandle Bind(OpaaxStringID InAction, EInputTrigger InTrigger,
                            TFunction<void(const InputActionValue&)> InCallback);

        /** Removes one binding. @return True if removed */
        bool Unbind(OpaaxStringID InAction, EInputTrigger InTrigger, DelegateHandle InHandle);

        /**
         * Removes every member binding owned by InOwner. Call it in the owner's Shutdown.
         * @return Number of bindings removed
         */
        Uint64 UnbindAll(void* InOwner);

        // =========================================================================
        // Query
        // =========================================================================
    public:
        /** This frame's value. Zero for an unknown action. */
        InputActionValue GetValue(OpaaxStringID InAction) const noexcept;

        /** This frame's state, or nullptr. */
        const InputActionState* FindState(OpaaxStringID InAction) const noexcept;

        Uint64 GetContextCount() const noexcept { return m_Evaluator.GetContextCount(); }
        Uint64 GetActionCount()  const noexcept { return m_Evaluator.GetActionCount(); }

        /** Visits every action and its state (editor Input panel). */
        template<typename TFunc>
        void ForEachAction(TFunc&& InFunc) const { m_Evaluator.ForEachAction(Forward<TFunc>(InFunc)); }

        /** Number of bound handlers. */
        Uint64 GetBindingCount() const noexcept;

        // =========================================================================
        // Keys consumed by the UI
        // =========================================================================
    public:
        /**
         * Marks keys as used this frame (by the UI). They drive no action. Cleared every frame.
         */
        void ConsumeThisFrame(const InputKeyMask& InConsumed);

        // =========================================================================
        // Override
        // =========================================================================
    public:
        //~Begin Subsystem interface
        /** After the UI, which reads the keys it uses first. */
        Int32 GetTickOrder() const override { return 200; }

        bool Startup() override;

        /** Evaluates, then calls the handlers of what fired. Before any world ticks. */
        void Update(double InDeltaTime) override;

        void Shutdown() override;
        //~End Subsystem interface

        // =========================================================================
        // Functions
        // =========================================================================
    private:
        /** The four delegates of an action, found or created. */
        FOnInputAction& DelegateFor(OpaaxStringID InAction, EInputTrigger InTrigger);

        /** Warns once per name when binding an unknown action. */
        void WarnIfUnknownAction(OpaaxStringID InAction, EInputTrigger InTrigger) const;

        /** A context asset as read, with the asset's keys. */
        struct KnownContext
        {
            InputContextLayout Layout;
            Int32              Priority = 0;

            /** One a slot, in the same order, with the asset's keys. */
            TDynArray<InputKeyBinding> Bindings;
        };

        /** A context added from an asset: rebinding its asset rebuilds it. */
        struct AddedContext
        {
            OpaaxStringID Name;
            OpaaxString   AssetPath;
        };

        /** Reads InAssetPath (its actions registered on first use) and keeps it known, read again if it was. */
        const KnownContext* LoadKnown(const OpaaxString& InAssetPath);

        /** The known context of InAssetPath (an old format's path too), or null. */
        const KnownContext* FindKnown(const OpaaxString& InAssetPath) const;

        /** InAssetPath as a known context is named: the file an old format's path was converted to. */
        static OpaaxString CanonicalPath(const OpaaxString& InAssetPath);

        /** InSlot with its context named as a known context is. */
        static InputKeySlot CanonicalSlot(const InputKeySlot& InSlot);

        /** InKnown's bindings with the player's keys; a mapping cleared drives nothing. */
        TDynArray<InputKeyBinding> BuildBindings(const KnownContext& InKnown) const;

        /** Every context added from an asset takes the player's keys. */
        void ApplyKeys();

        /** The keys changed: applied, written to the settings, told. */
        void KeysChanged();

        /** Reads the keys from the player's settings; those a known context does not allow are dropped. */
        void ReadKeys();

        /** Drops the keys InKnown does not allow, each with a warning. @return True if any was */
        bool DropUnknownKeys(const KnownContext& InKnown);

        /** A section of the settings changed: the keys are read again when it is theirs and they did not write it. */
        void HandleSettingsSectionChanged(OpaaxStringView InSection);

        /** Takes this frame's key for a capture, or Escape's cancel, and lets the key taken go once released. */
        void UpdateKeyCapture();

        /** Lets the keys held at the last ReleaseInput go once released. */
        void UpdateReleaseHeld();

        /** Ends the capture with InKey (None: cancelled) and tells it. */
        void EndKeyCapture(EKeyCode InKey);

        // =========================================================================
        // Members
        // =========================================================================
    private:
        struct ActionDelegates
        {
            OpaaxStringID Action;

            /** Indexed by EInputTrigger. */
            TFixedArray<FOnInputAction, INPUT_TRIGGER_COUNT> ByTrigger;
        };

        InputActionEvaluator m_Evaluator;

        TDynArray<ActionDelegates> m_Bindings;

        /** Keys used by the UI this frame. Cleared after Evaluate. */
        InputKeyMask m_PreConsumed{};

        // The player's keys over the context assets read so far, and the contexts added from them.
        InputRebinds            m_Rebinds;
        TDynArray<KnownContext> m_Known;
        TDynArray<AddedContext> m_Added;

        FInputKeyCheck      m_KeyCheck;
        FOnInputKeysChanged m_OnKeysChanged;

        /** The settings the keys are kept in (the game's), set at Startup; true while the keys write them. */
        IUserSettings* m_Settings     = nullptr;
        bool           m_bWritingKeys = false;

        // A capture: whether it waits, the keys held when it began (taken once released), the key it
        // took while that is still held, and the key the last one took.
        bool                m_bCapturing     = false;
        InputKeyMask        m_CaptureIgnored{};
        EKeyCode            m_CapturedHeld   = EKeyCode::None;
        TOptional<EKeyCode> m_LastCaptured;
        FOnKeyCaptured      m_OnKeyCaptured;

        // Holds under way (HoldInput), and the keys down at the last release, masked until released.
        Uint32       m_Holds = 0;
        InputKeyMask m_ReleaseHeldKeys{};
        bool         m_bReleaseHeld = false;
    };
}
