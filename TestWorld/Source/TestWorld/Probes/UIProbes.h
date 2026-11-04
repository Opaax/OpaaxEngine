#pragma once

#include "Core/String/OpaaxStringJson.h"
#include "Input/InputKeyNames.h"
#include "Input/Mapping/InputMappingSubsystem.h"
#include "UI/UIBinding.h"
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
}
