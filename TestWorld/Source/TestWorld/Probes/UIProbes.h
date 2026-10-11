#pragma once

#include "Core/String/OpaaxStringJson.h"
#include "Input/Mapping/InputMappingSubsystem.h"
#include "UI/UIBinding.h"
#include "UI/UISubsystem.h"
#include "UI/Widgets/UIButton.h"
#include "UI/Widgets/UIText.h"
#include "World/Behaviour/Behaviour.h"
#include "World/Systems/WorldContext.h"

namespace TestWorld
{
    // =============================================================================
    // UIProbe — mounts the canvas Asset on the game's UI while it lives. Counts the clicks on its
    //   ClickButton, which its Counter text shows through a binding ("Probe.Clicks"), and the Click
    //   action (the left button, Input/UITest.opaaxinputmap) the game itself gets: a click the UI
    //   handled does not reach the game.
    // =============================================================================
    class UIProbe : public Opaax::Behaviour
    {
    public:
        Opaax::OpaaxString Asset = "UI/Probe.opaaxui";

        bool               bMounted   = false;
        Opaax::Int32       Widgets    = 0;
        Opaax::Int32       Clicks     = 0;
        Opaax::Int32       GameClicks = 0;
        Opaax::OpaaxString CounterText;

        OPAAX_PROPERTIES(UIProbe, OPAAX_PROP(Asset), OPAAX_PROP(bMounted), OPAAX_PROP(Widgets), OPAAX_PROP(Clicks),
                         OPAAX_PROP(GameClicks), OPAAX_PROP(CounterText))

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
                lActions->AddContextAsset(OPAAX_ID("UITest"), Opaax::OpaaxString("Input/UITest.opaaxinputmap"));
            }
            BindAction<&UIProbe::OnGameClick>(OPAAX_ID("Click"), Opaax::EInputTrigger::Started);
        }

        void OnUpdate(float) override
        {
            if (m_Counter != nullptr)
            {
                CounterText = m_Counter->GetDisplayText();
            }
        }

        void OnDestroy() override
        {
            if (Opaax::InputMappingSubsystem* const lActions = GetContext().Actions)
            {
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

        static Opaax::Int32 CountWidgets(const Opaax::UIWidget& InWidget)
        {
            Opaax::Int32 lCount = 1;
            for (const Opaax::TUniquePtr<Opaax::UIWidget>& lChild : InWidget.GetChildren())
            {
                lCount += CountWidgets(*lChild);
            }
            return lCount;
        }

        Opaax::UIWidget*       m_Tree    = nullptr;   // owned by the game's canvas
        Opaax::UIText*         m_Counter = nullptr;
        Opaax::UIBindingHandle m_Binding;
    };
}
