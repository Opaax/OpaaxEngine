// Suite: DevConsole — the console fed a frame of input at a time: the toggle key, the text typed, the
// edit keys, Enter running a line through its own runner; help and console.info; none in a ship build.
#include <doctest.h>

#include "Commands/CommandRegistry.h"
#include "DevConsole/DevConsole.h"
#include "DevConsole/DevConsoleCommands.h"
#include "Input/InputManager.h"

using namespace Opaax;

namespace
{
    /** A console over a registry with one cheat, probe.scale {scale}, and the console's commands. */
    struct ConsoleGame
    {
        ConsoleGame()
        {
            Commands.Add(CommandSpec("probe.scale", "Sets a scale. Then nothing else.")
                             .Param("scale", ECommandParamType::Float, "the scale")
                             .Cheat(),
                         [this](const nlohmann::json& InParams)
                         {
                             Scale = InParams.value("scale", 1.f);
                             return CommandResult::Ok({ { "scale", Scale } });
                         });
            Console = MakeUnique<DevConsole>(Commands, Settings);
            Handles = RegisterDevConsoleCommands(Commands, *Console);
        }

        /** One frame: the console reads the input and runs its queue, then the frame ends. */
        void Frame()
        {
            Console->HandleInput(Input);
            Console->Tick();
            Input.EndFrame();
        }

        void Tap(const EKeyCode InKey)
        {
            Input.OnKeyPressed(InKey, false);
            Frame();
            Input.OnKeyReleased(InKey);
            Frame();
        }

        /** Types InText (ASCII) in one frame. */
        void Type(const char* InText)
        {
            for (const char* lChar = InText; *lChar != '\0'; ++lChar)
            {
                Input.OnCharTyped(static_cast<Uint32>(*lChar));
            }
            Frame();
        }

        /** Opens the console, types InLine and presses Enter. */
        void Run(const char* InLine)
        {
            if (!Console->IsOpen())
            {
                Tap(EKeyCode::GraveAccent);
            }
            Type(InLine);
            Tap(EKeyCode::Enter);
        }

        const DevConsoleRow& LastRow() const { return Console->GetOutput().GetRows().back(); }

        CommandRegistry          Commands{ true };
        InputManager             Input;
        ConsoleSettings          Settings;
        TUniquePtr<DevConsole>   Console;
        TDynArray<CommandHandle> Handles;
        float                    Scale = 1.f;
    };
}

TEST_CASE("DevConsole: the toggle key opens and closes it; the character it types is dropped")
{
    ConsoleGame lGame;
    CHECK_FALSE(lGame.Console->IsOpen());

    // The key types its own character in the same frame.
    lGame.Input.OnKeyPressed(EKeyCode::GraveAccent, false);
    lGame.Input.OnCharTyped(0xB2);   // a French keyboard's square
    lGame.Frame();
    CHECK(lGame.Console->IsOpen());
    CHECK(lGame.Console->GetLine().IsEmpty());

    lGame.Input.OnKeyReleased(EKeyCode::GraveAccent);
    lGame.Frame();
    lGame.Tap(EKeyCode::GraveAccent);
    CHECK_FALSE(lGame.Console->IsOpen());
}

TEST_CASE("DevConsole: closed, it takes no text and no key")
{
    ConsoleGame lGame;
    lGame.Type("abc");
    lGame.Tap(EKeyCode::Enter);
    CHECK(lGame.Console->GetLine().IsEmpty());
    CHECK(lGame.Console->GetOutput().GetRows().empty());
}

TEST_CASE("DevConsole: Enter runs the line; the echo, the answer and the history follow; a cheat is marked")
{
    ConsoleGame lGame;
    lGame.Run("probe.scale 0.25");

    CHECK(lGame.Scale == doctest::Approx(0.25f));
    CHECK(lGame.Console->GetLine().IsEmpty());
    CHECK(lGame.Commands.HaveCheatsRun());

    const auto& lRows = lGame.Console->GetOutput().GetRows();
    REQUIRE(lRows.size() >= 2);
    CHECK(lRows[0].Kind == EDevConsoleRow::Input);
    CHECK(lRows[0].Text == "> probe.scale 0.25");
    CHECK(lRows[1].Kind == EDevConsoleRow::Answer);

    REQUIRE(lGame.Console->GetHistory().GetLines().size() == 1);
    CHECK(lGame.Console->ConsumeHistoryChanged());
    CHECK_FALSE(lGame.Console->ConsumeHistoryChanged());
}

TEST_CASE("DevConsole: a line that does not read is an error row, and nothing runs")
{
    ConsoleGame lGame;
    lGame.Run("probe.scale fast");
    CHECK(lGame.Scale == 1.f);
    CHECK(lGame.LastRow().Kind == EDevConsoleRow::Error);
    CHECK(lGame.LastRow().Text.Contains("scale"));
    CHECK_FALSE(lGame.Commands.HaveCheatsRun());

    lGame.Run("no.such.command");
    CHECK(lGame.LastRow().Kind == EDevConsoleRow::Error);
    CHECK(lGame.LastRow().Text.Contains("no.such.command"));

    // An empty line does nothing at all.
    const Uint64 lRows = lGame.Console->GetOutput().GetRows().size();
    lGame.Tap(EKeyCode::Enter);
    CHECK(lGame.Console->GetOutput().GetRows().size() == lRows);
}

TEST_CASE("DevConsole: the edit keys move the caret and delete; a held key repeats")
{
    ConsoleGame lGame;
    lGame.Tap(EKeyCode::GraveAccent);
    lGame.Type("abcd");

    lGame.Tap(EKeyCode::Left);
    lGame.Tap(EKeyCode::Backspace);
    CHECK(lGame.Console->GetLine().GetText() == "abd");

    lGame.Tap(EKeyCode::Home);
    lGame.Tap(EKeyCode::Delete);
    CHECK(lGame.Console->GetLine().GetText() == "bd");

    lGame.Tap(EKeyCode::End);
    lGame.Type("e");
    CHECK(lGame.Console->GetLine().GetText() == "bde");

    // Backspace held: one press, then the OS repeats it.
    lGame.Input.OnKeyPressed(EKeyCode::Backspace, false);
    lGame.Frame();
    lGame.Input.OnKeyPressed(EKeyCode::Backspace, true);
    lGame.Frame();
    lGame.Input.OnKeyPressed(EKeyCode::Backspace, true);
    lGame.Frame();
    lGame.Input.OnKeyReleased(EKeyCode::Backspace);
    lGame.Frame();
    CHECK(lGame.Console->GetLine().IsEmpty());
}

TEST_CASE("DevConsole: Up and Down walk the history; Tab completes a name and keeps the text after the caret")
{
    ConsoleGame lGame;
    lGame.Run("probe.scale 2");
    lGame.Run("probe.scale 3");

    lGame.Type("draft");
    lGame.Tap(EKeyCode::Up);
    CHECK(lGame.Console->GetLine().GetText() == "probe.scale 3");
    lGame.Tap(EKeyCode::Up);
    CHECK(lGame.Console->GetLine().GetText() == "probe.scale 2");
    lGame.Tap(EKeyCode::Down);
    lGame.Tap(EKeyCode::Down);
    CHECK(lGame.Console->GetLine().GetText() == "draft");

    lGame.Console->GetLine().SetText("probe.sc 4");
    lGame.Console->GetLine().SetCaret(8);
    lGame.Tap(EKeyCode::Tab);
    CHECK(lGame.Console->GetLine().GetText() == "probe.scale  4");
    CHECK(lGame.Console->GetLine().GetCaret() == 12);
}

TEST_CASE("DevConsole: Tab with several matches lists them under the line")
{
    ConsoleGame lGame;
    lGame.Tap(EKeyCode::GraveAccent);
    lGame.Type("c");
    lGame.Tap(EKeyCode::Tab);

    // commands.list, commands.has, console.info, console.run, cheats.info: listed, the line kept.
    CHECK(lGame.Console->GetLine().GetText() == "c");
    REQUIRE_FALSE(lGame.Console->GetOutput().GetRows().empty());
    CHECK(lGame.LastRow().Kind == EDevConsoleRow::Info);
    CHECK(lGame.LastRow().Text.Contains("console.info"));
}

TEST_CASE("DevConsole: Escape closes it; the line stays for the next time")
{
    ConsoleGame lGame;
    lGame.Tap(EKeyCode::GraveAccent);
    lGame.Type("probe");
    lGame.Tap(EKeyCode::Escape);
    CHECK_FALSE(lGame.Console->IsOpen());

    lGame.Tap(EKeyCode::GraveAccent);
    CHECK(lGame.Console->GetLine().GetText() == "probe");
}

TEST_CASE("DevConsole: a line that waits answers when its wait ends, without holding the console's input")
{
    ConsoleGame lGame;
    lGame.Run("frames.wait 3");
    CHECK(lGame.Console->IsBusy());

    lGame.Type("x");
    CHECK(lGame.Console->GetLine().GetText() == "x");

    for (int lFrame = 0; lFrame < 4; ++lFrame)
    {
        lGame.Frame();
    }
    CHECK_FALSE(lGame.Console->IsBusy());
}

TEST_CASE("DevConsole: log lines are rows coloured by level; Page Up scrolls back")
{
    ConsoleGame lGame;
    lGame.Console->AddLog(ELogLevel::Warn, "careful");
    lGame.Console->AddLog(ELogLevel::Critical, "broken");
    lGame.Console->AddLog(ELogLevel::Trace, "detail");

    const auto& lRows = lGame.Console->GetOutput().GetRows();
    REQUIRE(lRows.size() == 3);
    CHECK(lRows[0].Kind == EDevConsoleRow::Warning);
    CHECK(lRows[1].Kind == EDevConsoleRow::Error);
    CHECK(lRows[2].Kind == EDevConsoleRow::Info);

    lGame.Console->SetPageRows(2);
    lGame.Tap(EKeyCode::GraveAccent);
    lGame.Tap(EKeyCode::PageUp);
    CHECK(lGame.Console->GetScroll() == 2);
    lGame.Tap(EKeyCode::PageUp);
    CHECK(lGame.Console->GetScroll() == 2);   // the oldest row is at the bottom
    lGame.Tap(EKeyCode::PageDown);
    CHECK(lGame.Console->GetScroll() == 0);
}

TEST_CASE("DevConsole: help lists the commands or describes one; console.info tells the state")
{
    ConsoleGame lGame;

    const OpaaxString lList = FormatHelpList(lGame.Commands.GetSpecs());
    CHECK(lList.Contains("probe.scale"));
    CHECK(lList.Contains("Sets a scale. [cheat]"));
    CHECK_FALSE(lList.Contains("Then nothing else"));

    lGame.Run("help probe.scale");
    CHECK(lGame.LastRow().Kind == EDevConsoleRow::Answer);
    CHECK(lGame.LastRow().Text == "  scale (float): the scale");

    lGame.Run("help nope");
    CHECK(lGame.LastRow().Kind == EDevConsoleRow::Error);

    const TSharedPtr<const RegisteredCommand> lInfo = lGame.Commands.Find("console.info");
    REQUIRE(lInfo != nullptr);
    const CommandResult lResult = lInfo->Handler({ { "rows", 2 } });
    REQUIRE(lResult.bOk);
    CHECK(lResult.Value["open"] == true);
    CHECK(lResult.Value["rows"].size() == 2);
    CHECK(lResult.Value["rows"][1]["kind"] == "error");
}

TEST_CASE("DevConsole: a ship build's registry takes none of the console's commands")
{
    CommandRegistry lShip(false);
    CHECK_FALSE(lShip.AllowsDevOnly());
    CHECK_FALSE(ICommandRegistry::Null().AllowsDevOnly());

    ConsoleSettings lSettings;
    DevConsole      lConsole(lShip, lSettings);

    const TDynArray<CommandHandle> lHandles = RegisterDevConsoleCommands(lShip, lConsole);
    for (const CommandHandle& lHandle : lHandles)
    {
        CHECK_FALSE(lHandle.IsValid());
    }
    CHECK_FALSE(lShip.Has("help"));
    CHECK_FALSE(lShip.Has("console.info"));
}
