#include "Commands/ValidateSandboxCommand.h"

#include "Application/Services/ILogger.h"   // OPAAX_LOG + LogCategory
#include "Editor/EditorContext.h"
#include "World/WorldManager.h"
#include "World/World.h"
#include "World/Entity/EntityMeta.h"

#include "Components/HealthComponent.h"
#include "Components/TagsComponent.h"

// OPAAX_LOG expands to an unqualified ToSpdLevel(...) — bring Opaax into scope, as SandboxPanel does.
using namespace Opaax;

namespace
{
    constexpr LogCategory LogSandboxEditorModule{"SandboxEditorModule"};
}

namespace SandboxEditor
{
    void ValidateSandboxCommand::Execute(Opaax::Editor::EditorContext& InContext, const Params&)
    {
        World* lWorld = InContext.Worlds.GetActiveWorld();
        if (lWorld == nullptr)
        {
            OPAAX_LOG(LogSandboxEditorModule, Warn, "Validate Sandbox: no active world");
            return;
        }

        // The tag count is HIERARCHICAL (I14): an entity tagged "Sandbox.Quad.White" answers to
        // "Sandbox", which nothing ever stored. This is the only place the MATCH — as opposed to
        // tag storage — is exercised in the running app.
        static const OpaaxTag lSandboxTag("Sandbox");

        Uint64 lTotal   = 0;
        Uint64 lMissing = 0;
        Uint64 lTagged  = 0;
        lWorld->Each<EntityMeta>(
            [&](auto InEntity, const EntityMeta&)
            {
                ++lTotal;
                if (!lWorld->GetRegistry().all_of<Sandbox::HealthComponent>(InEntity))
                {
                    ++lMissing;
                }

                const Sandbox::TagsComponent* lTags =
                    lWorld->GetRegistry().try_get<Sandbox::TagsComponent>(InEntity);

                if (lTags != nullptr && lTags->Tags.HasTag(lSandboxTag))
                {
                    ++lTagged;
                }
            });

        OPAAX_LOG(LogSandboxEditorModule, Info,
            "Validate Sandbox: world '{}' — {} entity(ies), {} without a HealthComponent, "
            "{} matching tag '{}'",
            lWorld->GetName().CStr(), lTotal, lMissing, lTagged, lSandboxTag);
    }
}
