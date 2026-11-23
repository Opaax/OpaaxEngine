#pragma once

#include "Core/String/OpaaxStringJson.h"
#include "Input/InputKeyNames.h"
#include "Input/Mapping/InputMappingSubsystem.h"
#include "UI/UIBinding.h"
#include "UI/Modifiers/UITweenModifier.h"
#include "UI/UILayerSettings.h"
#include "UI/UISubsystem.h"
#include "UI/Widgets/UIButton.h"
#include "UI/Widgets/UIScrollBox.h"
#include "UI/Widgets/UIText.h"
#include "World/Behaviour/Behaviour.h"
#include "World/Systems/WorldContext.h"

namespace TestWorld
{
    // =============================================================================
    // UIProbe — mounts the canvas Asset on the game's UI while it lives. Counts the clicks on its
    //   ClickButton, which its Counter text shows through a binding ("Probe.Clicks"), and the Click
    //   action (the left button, Input/UITest.opaaxasset) the game itself gets: a click the UI
    //   handled does not reach the game. bCapture waits for a key as a controls menu does: the UI
    //   gets no click while it waits, and Captured names the key taken.
    // =============================================================================
    class UIProbe : public Opaax::Behaviour
    {
        OPAAX_OBJECT(UIProbe, Opaax::Behaviour)

    public:
        Opaax::OpaaxString Asset = "UI/Probe.opaaxui";

        bool               bMounted   = false;
        Opaax::Int32       Widgets    = 0;
        Opaax::Int32       Clicks     = 0;
        Opaax::Int32       GameClicks = 0;
        Opaax::OpaaxString CounterText;

        /** Set by a test: the next frame waits for a key (BeginKeyCapture), then it goes back to false. */
        bool               bCapture   = false;
        bool               bCapturing = false;
        Opaax::OpaaxString Captured;   // the key the last capture took, "None" when cancelled

        OPAAX_PROPERTIES(UIProbe, OPAAX_PROP(Asset), OPAAX_PROP(bMounted), OPAAX_PROP(Widgets), OPAAX_PROP(Clicks),
                         OPAAX_PROP(GameClicks), OPAAX_PROP(CounterText), OPAAX_PROP(bCapture), OPAAX_PROP(bCapturing),
                         OPAAX_PROP(Captured))

        void OnStart() override
        {
            Opaax::UISubsystem* const lUI = GetContext().UI;
            if (lUI == nullptr)
            {
                return;
            }

            m_Tree   = lUI->MountAsset(Asset);
            bMounted = (m_Tree != nullptr);
            if (!bMounted)
            {
                return;
            }

            Widgets = CountWidgets(*m_Tree);
            if (auto* lButton = dynamic_cast<Opaax::UIButton*>(m_Tree->FindByName(Opaax::OpaaxString("ClickButton"))))
            {
                lButton->OnClick.AddMember(this, &UIProbe::OnClicked);
            }
            m_Counter = dynamic_cast<Opaax::UIText*>(m_Tree->FindByName(Opaax::OpaaxString("Counter")));
            m_Binding = lUI->GetCanvas().Bindings().Add(OPAAX_ID("Probe"), Opaax::MakeBindingReader(*this));

            if (Opaax::InputMappingSubsystem* const lActions = GetContext().Actions)
            {
                lActions->AddContextAsset(OPAAX_ID("UITest"), Opaax::OpaaxString("Input/UITest.opaaxasset"));
                lActions->OnKeyCaptured().AddMember(this, &UIProbe::OnKeyCaptured);
            }
            BindAction<&UIProbe::OnGameClick>(OPAAX_ID("Click"), Opaax::EInputTrigger::Started);
        }

        void OnUpdate(float) override
        {
            if (m_Counter != nullptr)
            {
                CounterText = m_Counter->GetDisplayText();
            }

            if (Opaax::InputMappingSubsystem* const lActions = GetContext().Actions)
            {
                if (bCapture)
                {
                    lActions->BeginKeyCapture();
                    bCapture = false;
                }
                bCapturing = lActions->IsCapturingKey();
            }
        }

        void OnDestroy() override
        {
            if (Opaax::InputMappingSubsystem* const lActions = GetContext().Actions)
            {
                lActions->OnKeyCaptured().RemoveAll(this);
                lActions->CancelKeyCapture();
                lActions->RemoveContext(OPAAX_ID("UITest"));
            }

            Opaax::UISubsystem* const lUI = GetContext().UI;
            if (lUI != nullptr && m_Tree != nullptr)
            {
                lUI->GetCanvas().Bindings().Remove(m_Binding);
                lUI->GetCanvas().Root().RemoveChild(*m_Tree);
            }
            m_Tree    = nullptr;
            m_Counter = nullptr;
        }

    private:
        void OnClicked() { ++Clicks; }

        void OnGameClick(const Opaax::InputActionValue&) { ++GameClicks; }

        void OnKeyCaptured(const Opaax::EKeyCode InKey) { Captured = Opaax::OpaaxString(ToString(InKey)); }

        static Opaax::Int32 CountWidgets(const Opaax::UIWidget& InWidget)
        {
            Opaax::Int32 lCount = 1;
            for (const Opaax::TStrongObjectPtr<Opaax::UIWidget>& lChild : InWidget.GetChildren())
            {
                lCount += CountWidgets(*lChild);
            }
            return lCount;
        }

        Opaax::UIWidget*       m_Tree    = nullptr;   // owned by the game's canvas
        Opaax::UIText*         m_Counter = nullptr;
        Opaax::UIBindingHandle m_Binding;
    };

    // =============================================================================
    // ScrollProbe — mounts the canvas Asset, a scroll box named List, while it lives, and records how far
    //   the list is scrolled and whether a drag moves it. Counts the Notch action (the wheel's keys,
    //   Input/Wheel.opaaxasset) the game itself gets: a notch the list scrolled does not reach the game.
    // =============================================================================
    class ScrollProbe : public Opaax::Behaviour
    {
        OPAAX_OBJECT(ScrollProbe, Opaax::Behaviour)

    public:
        Opaax::OpaaxString Asset = "UI/Scroll.opaaxui";

        bool         bMounted    = false;
        float        Scroll      = 0.f;
        float        MaxScroll   = 0.f;
        bool         bDragging   = false;
        Opaax::Int32 GameNotches = 0;

        OPAAX_PROPERTIES(ScrollProbe, OPAAX_PROP(Asset), OPAAX_PROP(bMounted), OPAAX_PROP(Scroll), OPAAX_PROP(MaxScroll),
                         OPAAX_PROP(bDragging), OPAAX_PROP(GameNotches))

        void OnStart() override
        {
            Opaax::UISubsystem* const lUI = GetContext().UI;
            if (lUI == nullptr)
            {
                return;
            }

            m_Tree   = lUI->MountAsset(Asset);
            bMounted = (m_Tree != nullptr);
            if (!bMounted)
            {
                return;
            }

            m_List = dynamic_cast<Opaax::UIScrollBox*>(m_Tree->FindByName(Opaax::OpaaxString("List")));

            if (Opaax::InputMappingSubsystem* const lActions = GetContext().Actions)
            {
                lActions->AddContextAsset(OPAAX_ID("Wheel"), Opaax::OpaaxString("Input/Wheel.opaaxasset"));
            }
            BindAction<&ScrollProbe::OnNotch>(OPAAX_ID("Notch"), Opaax::EInputTrigger::Started);
        }

        void OnUpdate(float) override
        {
            if (m_List != nullptr)
            {
                Scroll    = m_List->GetScroll();
                MaxScroll = m_List->GetMaxScroll();
                bDragging = m_List->IsDragging();
            }
        }

        void OnDestroy() override
        {
            if (Opaax::InputMappingSubsystem* const lActions = GetContext().Actions)
            {
                lActions->RemoveContext(OPAAX_ID("Wheel"));
            }

            Opaax::UISubsystem* const lUI = GetContext().UI;
            if (lUI != nullptr && m_Tree != nullptr)
            {
                lUI->GetCanvas().Root().RemoveChild(*m_Tree);
            }
            m_Tree = nullptr;
            m_List = nullptr;
        }

    private:
        void OnNotch(const Opaax::InputActionValue&) { ++GameNotches; }

        Opaax::UIWidget*    m_Tree = nullptr;   // owned by the game's canvas
        Opaax::UIScrollBox* m_List = nullptr;
    };

    // =============================================================================
    // ModifierProbe — mounts the canvas Asset (UI/Modifiers.opaaxui) while it lives, counts the clicks
    //   on its two moved buttons (HitOn's pose moves its hit area with it, HitOff's does not) and the
    //   tweens that reach their end (OnFinished of every tween modifier in the canvas).
    // =============================================================================
    class ModifierProbe : public Opaax::Behaviour
    {
        OPAAX_OBJECT(ModifierProbe, Opaax::Behaviour)

    public:
        Opaax::OpaaxString Asset = "UI/Modifiers.opaaxui";

        bool         bMounted       = false;
        Opaax::Int32 HitOnClicks    = 0;
        Opaax::Int32 HitOffClicks   = 0;
        Opaax::Int32 TweensFinished = 0;

        OPAAX_PROPERTIES(ModifierProbe, OPAAX_PROP(Asset), OPAAX_PROP(bMounted), OPAAX_PROP(HitOnClicks),
                         OPAAX_PROP(HitOffClicks), OPAAX_PROP(TweensFinished))

        void OnStart() override
        {
            Opaax::UISubsystem* const lUI = GetContext().UI;
            if (lUI == nullptr)
            {
                return;
            }

            m_Tree   = lUI->MountAsset(Asset);
            bMounted = (m_Tree != nullptr);
            if (!bMounted)
            {
                return;
            }

            BindTweens(*m_Tree);

            if (auto* lOn = dynamic_cast<Opaax::UIButton*>(m_Tree->FindByName(Opaax::OpaaxString("HitOn"))))
            {
                lOn->OnClick.AddMember(this, &ModifierProbe::OnHitOn);
            }
            if (auto* lOff = dynamic_cast<Opaax::UIButton*>(m_Tree->FindByName(Opaax::OpaaxString("HitOff"))))
            {
                lOff->OnClick.AddMember(this, &ModifierProbe::OnHitOff);
            }
        }

        void OnDestroy() override
        {
            Opaax::UISubsystem* const lUI = GetContext().UI;
            if (lUI != nullptr && m_Tree != nullptr)
            {
                lUI->GetCanvas().Root().RemoveChild(*m_Tree);
            }
            m_Tree = nullptr;
        }

    private:
        void OnHitOn()  { ++HitOnClicks; }
        void OnHitOff() { ++HitOffClicks; }
        void OnTweenFinished() { ++TweensFinished; }

        /** Hears every tween modifier under InWidget. The mount already played the Show ones: they finish later. */
        void BindTweens(Opaax::UIWidget& InWidget)
        {
            for (const Opaax::TStrongObjectPtr<Opaax::UIModifier>& lModifier : InWidget.GetModifiers())
            {
                if (auto* lTween = Opaax::Cast<Opaax::UITweenModifier>(lModifier.Get()))
                {
                    lTween->OnFinished.AddMember(this, &ModifierProbe::OnTweenFinished);
                }
            }
            for (const Opaax::TStrongObjectPtr<Opaax::UIWidget>& lChild : InWidget.GetChildren())
            {
                BindTweens(*lChild);
            }
        }

        Opaax::UIWidget* m_Tree = nullptr;   // owned by the game's canvas
    };

    // =============================================================================
    // LayerProbe — mounts three canvases in layers, the top one first: LayerDebug in Debug, LayerMenu
    //   in Menu, LayerPause in UI.Layer.Menu.Pause (TestWorld's Engine.config lists it under Menu's own trees,
    //   its Tags.config registers it).
    //   Records the order of the canvas root's trees (DrawOrder, bottom to top), the widget hovered, the
    //   menu button's state and the clicks on each button: where the debug button covers the menu button,
    //   only the debug one hears the pointer.
    // =============================================================================
    class LayerProbe : public Opaax::Behaviour
    {
        OPAAX_OBJECT(LayerProbe, Opaax::Behaviour)

    public:
        bool               bMounted    = false;
        Opaax::OpaaxString DrawOrder;
        Opaax::OpaaxString HoveredName;
        Opaax::OpaaxString MenuState;
        Opaax::Int32       MenuClicks  = 0;
        Opaax::Int32       FreeClicks  = 0;
        Opaax::Int32       DebugClicks = 0;

        OPAAX_PROPERTIES(LayerProbe, OPAAX_PROP(bMounted), OPAAX_PROP(DrawOrder), OPAAX_PROP(HoveredName),
                         OPAAX_PROP(MenuState), OPAAX_PROP(MenuClicks), OPAAX_PROP(FreeClicks), OPAAX_PROP(DebugClicks))

        void OnStart() override
        {
            Opaax::UISubsystem* const lUI = GetContext().UI;
            if (lUI == nullptr)
            {
                return;
            }

            m_Debug  = lUI->MountAsset(Opaax::OpaaxString("UI/LayerDebug.opaaxui"), nullptr, Opaax::UILayerDebug());
            m_Menu   = lUI->MountAsset(Opaax::OpaaxString("UI/LayerMenu.opaaxui"), nullptr, Opaax::UILayerMenu());
            m_Pause  = lUI->MountAsset(Opaax::OpaaxString("UI/LayerPause.opaaxui"), nullptr, Opaax::OpaaxTag("UI.Layer.Menu.Pause"));
            bMounted = m_Debug != nullptr && m_Menu != nullptr && m_Pause != nullptr;
            if (!bMounted)
            {
                return;
            }

            m_MenuButton = dynamic_cast<Opaax::UIButton*>(m_Menu->FindByName(Opaax::OpaaxString("MenuButton")));
            if (m_MenuButton != nullptr)
            {
                m_MenuButton->OnClick.AddMember(this, &LayerProbe::OnMenu);
            }
            if (auto* lFree = dynamic_cast<Opaax::UIButton*>(m_Menu->FindByName(Opaax::OpaaxString("MenuFree"))))
            {
                lFree->OnClick.AddMember(this, &LayerProbe::OnFree);
            }
            if (auto* lDebug = dynamic_cast<Opaax::UIButton*>(m_Debug->FindByName(Opaax::OpaaxString("DebugButton"))))
            {
                lDebug->OnClick.AddMember(this, &LayerProbe::OnDebug);
            }
        }

        void OnUpdate(float) override
        {
            Opaax::UISubsystem* const lUI = GetContext().UI;
            if (lUI == nullptr || !bMounted)
            {
                return;
            }

            Opaax::OpaaxString lOrder;
            for (const Opaax::TStrongObjectPtr<Opaax::UIWidget>& lTree : lUI->GetCanvas().Root().GetChildren())
            {
                lOrder += lOrder.IsEmpty() ? lTree->Name : "," + lTree->Name;
            }
            DrawOrder = lOrder;

            const Opaax::UIWidget* const lHovered = lUI->GetCanvas().GetHovered();
            HoveredName = lHovered != nullptr ? lHovered->Name : Opaax::OpaaxString();
            MenuState   = m_MenuButton != nullptr ? Opaax::OpaaxString(Opaax::ToString(m_MenuButton->GetInteractionState()))
                                                  : Opaax::OpaaxString();
        }

        void OnDestroy() override
        {
            Opaax::UISubsystem* const lUI = GetContext().UI;
            for (Opaax::UIWidget* const lTree : { m_Debug, m_Menu, m_Pause })
            {
                if (lUI != nullptr && lTree != nullptr)
                {
                    lUI->GetCanvas().Root().RemoveChild(*lTree);
                }
            }
            m_Debug      = nullptr;
            m_Menu       = nullptr;
            m_Pause      = nullptr;
            m_MenuButton = nullptr;
        }

    private:
        void OnMenu()  { ++MenuClicks; }
        void OnFree()  { ++FreeClicks; }
        void OnDebug() { ++DebugClicks; }

        Opaax::UIWidget* m_Debug      = nullptr;   // owned by the game's canvas
        Opaax::UIWidget* m_Menu       = nullptr;
        Opaax::UIWidget* m_Pause      = nullptr;
        Opaax::UIButton* m_MenuButton = nullptr;
    };
}
