#include "DevConsole/DevConsoleSubsystem.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IEngine.h"
#include "Application/Services/IPaths.h"
#include "Core/IO/FileIO.h"
#include "Core/IO/FileSystem.h"
#include "Core/Log/Logger.h"
#include "Core/Maths/Maths.h"
#include "DevConsole/DevConsole.h"
#include "DevConsole/DevConsoleCommands.h"
#include "Engine/Config/EngineConfigData.h"
#include "Engine/GameInstance/GameInstance.h"
#include "Engine/GameInstance/GameInstanceContext.h"
#include "Engine/GameInstance/GameInstanceManager.h"
#include "Input/Mapping/InputMappingSubsystem.h"
#include "UI/UICanvas.h"
#include "UI/UILayer.h"
#include "UI/Widgets/UIImage.h"
#include "UI/Widgets/UIText.h"
#include "World/WorldManager.h"

namespace Opaax
{
    namespace
    {
        /** Space around the console's text, canvas units. */
        constexpr float PADDING = 12.f;

        /** A row's height for a font size. */
        float RowHeight(const float InFontSize) { return InFontSize * 1.3f; }

        /** A rect stretched along the bottom of its parent, InFromBottom above it, InHeight tall. */
        UIRect BottomStrip(const float InFromBottom, const float InHeight)
        {
            UIRect lRect;
            lRect.AnchorMin        = { 0.f, 0.f };
            lRect.AnchorMax        = { 1.f, 0.f };
            lRect.Pivot            = { 0.5f, 0.f };
            lRect.AnchoredPosition = { 0.f, InFromBottom };
            lRect.SizeDelta        = { -2.f * PADDING, InHeight };
            return lRect;
        }
    }

    DevConsoleSubsystem::DevConsoleSubsystem()  = default;
    DevConsoleSubsystem::~DevConsoleSubsystem() = default;

    bool DevConsoleSubsystem::Startup()
    {
        GameInstanceContext& lContext = GetContext();

        // A ship build has no console: no canvas, no key, no commands.
        if (!lContext.Commands.AllowsDevOnly())
        {
            return true;
        }

        m_Settings    = lContext.Config.Console;
        m_HistoryPath = lContext.Paths.UserDir() + "/" + HISTORY_FILE;

        if (GameInstance* lGame = OpaaxApplication::GetAppService<IEngine>().GetGameInstances().GetGameInstance())
        {
            m_Mapping = lGame->GetSubsystems().GetSubsystem<InputMappingSubsystem>();
        }

        m_Console  = MakeShared<DevConsole>(lContext.Commands, m_Settings);
        m_Commands = RegisterDevConsoleCommands(lContext.Commands, *m_Console);
        LoadHistory();

        // From the first line logged, so what went wrong at startup shows when it opens.
        Logger::Get().EnsureHistory(LOG_HISTORY);
        m_LogSequence = 0;

        BuildCanvas();

        OPAAX_LOG(LogDevConsole, Trace, "Dev console ready: {} opens it", ToString(m_Settings.ToggleKey));
        return true;
    }

    void DevConsoleSubsystem::Update(double /*InDeltaTime*/)
    {
        if (m_Console == nullptr)
        {
            return;
        }

        PullLog();

        m_Console->HandleInput(GetContext().Input);

        // A line can end the game (editor.stop): Shutdown then runs inside the Tick.
        const TSharedPtr<DevConsole> lTicking = m_Console;
        lTicking->Tick();
        if (m_Console == nullptr)
        {
            return;
        }

        if (m_Console->IsOpen() != m_bWasOpen)
        {
            m_bWasOpen = m_Console->IsOpen();
            OnOpenChanged(m_bWasOpen);
        }

        if (m_Console->ConsumeHistoryChanged())
        {
            SaveHistory();
        }

        if (m_Console->IsOpen())
        {
            RefreshCanvas();
            OpaaxApplication::GetAppService<IEngine>().SubmitUICanvas(*m_Canvas, nullptr, nullptr, UILayer::Console);
        }
    }

    void DevConsoleSubsystem::Shutdown()
    {
        if (m_Console == nullptr)
        {
            return;
        }

        // The input mapping shut down first (it ticks later) and let go of every hold itself.
        m_Mapping = nullptr;
        if (m_bWasOpen)
        {
            OnOpenChanged(false);
            m_bWasOpen = false;
        }

        m_Commands.clear();
        m_Rows.clear();
        m_LineText = nullptr;
        m_Canvas.reset();
        m_Console.reset();
    }

    // =========================================================================
    // Internal
    // =========================================================================
    void DevConsoleSubsystem::BuildCanvas()
    {
        m_Canvas = MakeUnique<UICanvas>(REFERENCE_HEIGHT);

        const float lPanelHeight = Maths::Clamp(m_Settings.Height, 0.1f, 1.f) * REFERENCE_HEIGHT;
        const float lRowHeight   = RowHeight(m_Settings.FontSize);

        // The panel: across the top of the screen.
        auto lPanel  = NewObject<UIImage>();
        lPanel->Name = "Console";
        lPanel->SetColor(m_Settings.BackgroundColor);
        UIRect lPanelRect;
        lPanelRect.AnchorMin = { 0.f, 1.f - lPanelHeight / REFERENCE_HEIGHT };
        lPanelRect.AnchorMax = { 1.f, 1.f };
        lPanelRect.SizeDelta = { 0.f, 0.f };
        lPanel->SetRect(lPanelRect);
        UIWidget* lPanelWidget = m_Canvas->Root().AddChild(Move(lPanel));

        const auto lMakeText = [this](const OpaaxString& InName, const LinearColor& InColor)
        {
            auto lText  = NewObject<UIText>();
            lText->Name  = InName;
            lText->bWrap = false;
            lText->SetFont(FONT);
            lText->SetSize(m_Settings.FontSize);
            lText->SetColor(InColor);
            lText->SetAlign(ETextAlign::Left, EUIVAlign::Middle);
            return lText;
        };

        // The line, at the bottom, under a thin rule.
        auto lLine = lMakeText("Line", m_Settings.InputColor);
        lLine->SetRect(BottomStrip(PADDING, lRowHeight));
        m_LineText = static_cast<UIText*>(lPanelWidget->AddChild(Move(lLine)));

        auto lRule  = NewObject<UIImage>();
        lRule->Name = "Rule";
        lRule->SetColor({ m_Settings.TextColor.x, m_Settings.TextColor.y, m_Settings.TextColor.z, 0.25f });
        lRule->SetRect(BottomStrip(PADDING + lRowHeight + 4.f, 2.f));
        lPanelWidget->AddChild(Move(lRule));

        // The rows above it, the newest at the bottom: as many as fit.
        const float  lRowsFrom = PADDING + lRowHeight + 10.f;
        const Uint32 lCount    = static_cast<Uint32>(Maths::Max((lPanelHeight - lRowsFrom - PADDING) / lRowHeight, 1.f));
        for (Uint32 lIndex = 0; lIndex < lCount; ++lIndex)
        {
            auto lRow = lMakeText(OpaaxString::Format("Row{}", lIndex), m_Settings.TextColor);
            lRow->SetRect(BottomStrip(lRowsFrom + static_cast<float>(lIndex) * lRowHeight, lRowHeight));
            m_Rows.push_back(static_cast<UIText*>(lPanelWidget->AddChild(Move(lRow))));
        }
        m_Console->SetPageRows(lCount);
        m_bShownAny = false;
    }

    void DevConsoleSubsystem::RefreshCanvas()
    {
        const DevConsoleOutput& lOutput = m_Console->GetOutput();
        if (!m_bShownAny || lOutput.GetRevision() != m_ShownRevision || m_Console->GetScroll() != m_ShownScroll)
        {
            const auto&  lRows  = lOutput.GetRows();
            const Uint64 lCount = lRows.size();
            for (Uint64 lIndex = 0; lIndex < m_Rows.size(); ++lIndex)
            {
                // Row lIndex shows the output row lIndex above the newest shown.
                const Uint64 lBack = lIndex + m_Console->GetScroll();
                if (lBack >= lCount)
                {
                    m_Rows[lIndex]->SetText(OpaaxString());
                    continue;
                }

                const DevConsoleRow& lRow = lRows[lCount - 1 - lBack];
                m_Rows[lIndex]->SetText(lRow.Text);
                m_Rows[lIndex]->SetColor(lRow.Kind == EDevConsoleRow::Input   ? m_Settings.InputColor
                                       : lRow.Kind == EDevConsoleRow::Warning ? m_Settings.WarningColor
                                       : lRow.Kind == EDevConsoleRow::Error   ? m_Settings.ErrorColor
                                                                              : m_Settings.TextColor);
            }
            m_ShownRevision = lOutput.GetRevision();
            m_ShownScroll   = m_Console->GetScroll();
        }

        // The caret is a bar between the characters.
        const DevConsoleLineEdit& lEdit   = m_Console->GetLine();
        const OpaaxString         lBefore = lEdit.GetTextBeforeCaret();
        const OpaaxString         lLine   = "> " + lBefore + "|" + lEdit.GetText().SubString(lBefore.GetLength());
        if (!m_bShownAny || lLine != m_ShownLine)
        {
            m_LineText->SetText(lLine);
            m_ShownLine = lLine;
        }
        m_bShownAny = true;
    }

    void DevConsoleSubsystem::PullLog()
    {
        TDynArray<LogEntry> lEntries;
        m_LogSequence = Logger::Get().CopyHistorySince(m_LogSequence, lEntries);
        for (const LogEntry& lEntry : lEntries)
        {
            if (lEntry.Level >= m_Settings.LogLevel)
            {
                m_Console->AddLog(lEntry.Level, OpaaxString::Format("[{}] {}", lEntry.Category.CStr(), lEntry.Message.CStr()));
            }
        }
    }

    void DevConsoleSubsystem::OnOpenChanged(const bool bInOpen)
    {
        if (m_Mapping != nullptr)
        {
            if (bInOpen)
            {
                m_Mapping->HoldInput();
            }
            else
            {
                m_Mapping->ReleaseInput();
            }
        }

        WorldManager& lWorlds = GetContext().Worlds;
        if (bInOpen && m_Settings.bPauseWorld && !lWorlds.IsPaused())
        {
            lWorlds.SetPaused(true);
            m_bPausedWorld = true;
        }
        else if (!bInOpen && m_bPausedWorld)
        {
            lWorlds.SetPaused(false);
            m_bPausedWorld = false;
        }

        OPAAX_LOG(LogDevConsole, Trace, "Dev console {}", bInOpen ? "opened" : "closed");
    }

    void DevConsoleSubsystem::LoadHistory()
    {
        if (FileSystem::IsFile(m_HistoryPath))
        {
            m_Console->GetHistory().FromText(FileIO::ReadAllText(m_HistoryPath));
        }
    }

    void DevConsoleSubsystem::SaveHistory()
    {
        if (!FileIO::WriteAllTextAtomic(m_HistoryPath, m_Console->GetHistory().ToText()))
        {
            OPAAX_LOG(LogDevConsole, Warn, "The console's history could not be saved to '{}'", m_HistoryPath.CStr());
        }
    }
}
