#include "Systems/PauseMenuSubsystem.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IEngine.h"
#include "Application/Services/ILogger.h"
#include "Engine/Input/InputMappingSubsystem.h"
#include "Engine/Input/InputTypes.h"
#include "Engine/Subsystems/Input/InputCodes.h"
#include "Engine/UI/UISubsystem.h"
#include "UI/UICanvas.h"
#include "UI/Widgets/UIButton.h"
#include "UI/Widgets/UIImage.h"
#include "UI/Widgets/UIPanel.h"
#include "UI/Widgets/UIText.h"
#include "World/Systems/WorldContext.h"
#include "World/World.h"

using namespace Opaax;

namespace Sandbox
{
    namespace
    {
        constexpr LogCategory LogPauseMenu{"PauseMenu"};

        const OpaaxStringID kMenuToggleAction = OPAAX_ID("MenuToggle");

        constexpr const char* kFace = "/Engine/Fonts/Roboto/roboto-latin-700-normal.ttf";

        /** The two levels Next Level swaps between; the world's name says which one is up. */
        constexpr const char* kLevelMain    = "Levels/Main.opaaxlevel";
        constexpr const char* kLevelPhysics = "Levels/PhysicsTest.opaaxlevel";

        /** The modal's root: the focused widget while the menu is up, so Escape reaches it in UIOnly. */
        class PauseMenuPanel final : public UIWidget
        {
        public:
            TFunction<void()> OnEscape;

            OpaaxStringID GetTypeName() const noexcept override { return OPAAX_ID("PauseMenuPanel"); }

            EUIReply OnKeyEvent(const UIKeyEvent& InEvent) override
            {
                if (InEvent.bPressed && InEvent.Key == EKeyCode::Escape && OnEscape)
                {
                    OnEscape();
                    return EUIReply::Handled;
                }
                return EUIReply::Unhandled;
            }
        };

        UIRect Anchored(const Vector2F& InAnchor, const Vector2F& InOffset, const Vector2F& InSize)
        {
            UIRect lRect;
            lRect.AnchorMin = lRect.AnchorMax = lRect.Pivot = InAnchor;
            lRect.AnchoredPosition = InOffset;
            lRect.SizeDelta        = InSize;
            return lRect;
        }

        UIRect Stretched()
        {
            UIRect lRect;
            lRect.AnchorMin = { 0.f, 0.f };
            lRect.AnchorMax = { 1.f, 1.f };
            lRect.SizeDelta = { 0.f, 0.f };
            return lRect;
        }

        /** A button with a centred label, sized InSize, at InRect. */
        UIButton* AddButton(UIWidget& InParent, const char* InName, const char* InLabel, const UIRect& InRect, float InTextSize)
        {
            auto lButton  = MakeUnique<UIButton>();
            lButton->Name = InName;
            lButton->SetRect(InRect);

            auto lLabel = MakeUnique<UIText>();
            lLabel->Name = OpaaxString(InName) + "Label";
            lLabel->SetRect(Stretched());
            lLabel->bHitTestable = false;   // the button is the target, not its text
            lLabel->SetFont(kFace);
            lLabel->SetSize(InTextSize);
            lLabel->SetAlign(ETextAlign::Center, EUIVAlign::Middle);
            lLabel->SetText(InLabel);
            lButton->AddChild(std::move(lLabel));

            return static_cast<UIButton*>(InParent.AddChild(std::move(lButton)));
        }
    }

    bool PauseMenuSubsystem::ShouldCreate(const World& InWorld)
    {
        return InWorld.GetMode() == EWorldMode::Play;
    }

    bool PauseMenuSubsystem::Startup()
    {
        if (m_Context->UI == nullptr)
        {
            OPAAX_LOG(LogPauseMenu, Error, "Pause menu started with NO UI subsystem — this world has no game instance, nothing is drawn.");
            return true;
        }

        UICanvas& lCanvas = m_Context->UI->GetCanvas();

        // The HUD button — lives in GameAndUI: a click opens the menu, everything else still reaches the game.
        m_MenuButton = AddButton(lCanvas.Root(), "MenuButton", "Menu",
                                 Anchored({ 1.f, 1.f }, { -24.f, -24.f }, { 160.f, 56.f }), 32.f);
        m_MenuButton->OnClick.AddMember(this, &PauseMenuSubsystem::Open);

        // The modal, added AFTER the button so it draws over it and is hit-tested first.
        auto lMenu     = MakeUnique<PauseMenuPanel>();
        lMenu->Name    = "PauseMenu";
        lMenu->bVisible = false;
        lMenu->SetRect(Stretched());
        lMenu->OnEscape = [this]() { Close(); };

        auto lDim = MakeUnique<UIImage>();
        lDim->Name = "Dim";
        lDim->SetRect(Stretched());
        lDim->SetColor({ 0.f, 0.f, 0.f, 0.55f });
        lMenu->AddChild(std::move(lDim));

        auto lBox = MakeUnique<UIImage>();
        lBox->Name = "Box";
        lBox->SetRect(Anchored({ 0.5f, 0.5f }, { 0.f, 0.f }, { 520.f, 300.f }));
        lBox->SetColor({ 0.12f, 0.12f, 0.15f, 0.95f });
        UIWidget* lBoxWidget = lMenu->AddChild(std::move(lBox));

        auto lTitle = MakeUnique<UIText>();
        lTitle->Name = "Title";
        lTitle->SetRect(Anchored({ 0.5f, 1.f }, { 0.f, -24.f }, { 480.f, 80.f }));
        lTitle->bHitTestable = false;
        lTitle->SetFont(kFace);
        lTitle->SetSize(64.f);
        lTitle->SetAlign(ETextAlign::Center, EUIVAlign::Middle);
        lTitle->SetText("Paused");
        lBoxWidget->AddChild(std::move(lTitle));

        UIButton* lResume = AddButton(*lBoxWidget, "ResumeButton", "Resume",
                                      Anchored({ 0.5f, 0.f }, { 0.f, 110.f }, { 240.f, 64.f }), 36.f);
        lResume->OnClick.AddMember(this, &PauseMenuSubsystem::Close);

        UIButton* lNext = AddButton(*lBoxWidget, "NextLevelButton", "Next Level",
                                    Anchored({ 0.5f, 0.f }, { 0.f, 30.f }, { 240.f, 64.f }), 36.f);
        lNext->OnClick.AddMember(this, &PauseMenuSubsystem::NextLevel);

        m_Menu = lCanvas.Root().AddChild(std::move(lMenu));

        if (m_Context->Actions != nullptr)
        {
            m_Context->Actions->Bind(kMenuToggleAction, EInputTrigger::Started, this, &PauseMenuSubsystem::OnMenuToggle);
        }

        OPAAX_LOG(LogPauseMenu, Info, "Pause menu started — {} widget(s), mode {}",
                  10, ToString(m_Context->UI->GetInputMode()));
        return true;
    }

    void PauseMenuSubsystem::Shutdown()
    {
        if (m_Context->Actions != nullptr)
        {
            m_Context->Actions->UnbindAll(this);
        }

        if (m_Context->UI != nullptr)
        {
            // A world leaving with the menu up must not leave the session muted.
            if (m_bOpen)
            {
                m_Context->UI->SetInputMode(EUIInputMode::GameAndUI);
            }

            UIWidget& lRoot = m_Context->UI->GetCanvas().Root();
            if (m_Menu       != nullptr) { lRoot.RemoveChild(*m_Menu); }
            if (m_MenuButton != nullptr) { lRoot.RemoveChild(*m_MenuButton); }
        }

        m_Menu       = nullptr;
        m_MenuButton = nullptr;
        m_bOpen      = false;

        OPAAX_LOG(LogPauseMenu, Info, "Pause menu shutdown — opened {} time(s)", m_Opens);
    }

    void PauseMenuSubsystem::Open()
    {
        if (m_bOpen || m_Menu == nullptr || m_Context->UI == nullptr)
        {
            return;
        }

        m_bOpen = true;
        ++m_Opens;

        m_Menu->bVisible = true;
        m_Context->UI->GetCanvas().SetFocus(m_Menu);
        m_Context->UI->SetInputMode(EUIInputMode::UIOnly);

        OPAAX_LOG(LogPauseMenu, Info, "OPEN (UIOnly)");
    }

    void PauseMenuSubsystem::Close()
    {
        if (!m_bOpen || m_Menu == nullptr || m_Context->UI == nullptr)
        {
            return;
        }

        m_bOpen = false;

        m_Menu->bVisible = false;
        m_Context->UI->GetCanvas().SetFocus(nullptr);
        m_Context->UI->SetInputMode(EUIInputMode::GameAndUI);

        OPAAX_LOG(LogPauseMenu, Info, "CLOSE (GameAndUI)");
    }

    void PauseMenuSubsystem::NextLevel()
    {
        // A REQUEST, not a call: this runs inside the UI tick, and OpenLevel here would destroy the
        // world this subsystem belongs to from under its own button. The engine swaps at the next
        // frame's start, and the cover is up for this one (UI21).
        WorldSpec lSpec;
        lSpec.LevelPath = m_Context->OwningWorld.GetName() == OpaaxString("Main") ? kLevelPhysics : kLevelMain;
        lSpec.Mode      = EWorldMode::Play;

        OPAAX_LOG(LogPauseMenu, Info, "Next Level — '{}' requested from '{}'",
                  lSpec.LevelPath.CStr(), m_Context->OwningWorld.GetName().CStr());

        OpaaxApplication::GetAppService<IEngine>().RequestOpenLevel(lSpec);
    }

    void PauseMenuSubsystem::OnMenuToggle(const InputActionValue& /*InValue*/)
    {
        // Bindings outlive worlds (IM8). Only ever OPENS: while the menu is up the mapping is muted,
        // so closing is the focused panel's Escape or the Resume button.
        if (!m_Context->OwningWorld.IsActive())
        {
            return;
        }

        Open();
    }
}
