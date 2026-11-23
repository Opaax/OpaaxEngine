// The tags the engine's own systems name their data by. A game declares its own the same way, or lists
// them in its Tags.config.
#include "Engine/Registries/AutoRegistration.h"

// The game UI's layers (Engine.config UI.Layers): a higher order on top.
OPAAX_REGISTER_TAG("UI.Layer.Hud", "What is on screen while playing");
OPAAX_REGISTER_TAG("UI.Layer.Menu", "Menus: pause, options");
OPAAX_REGISTER_TAG("UI.Layer.Debug", "Debug panels, over the menus");
OPAAX_REGISTER_TAG("UI.Layer.Loading", "A game's own loading or fade screens");

// The mix tree (Engine.config Audio.Buses): Audio.Bus is the master, every bus under it.
OPAAX_REGISTER_TAG("Audio.Bus", "The master bus: every sound goes through it");
OPAAX_REGISTER_TAG("Audio.Bus.Music", "Music");
OPAAX_REGISTER_TAG("Audio.Bus.SFX", "Sound effects: the bus a sound plays in when it names none");
OPAAX_REGISTER_TAG("Audio.Bus.Dialogue", "Voices");
OPAAX_REGISTER_TAG("Audio.Bus.Ambience", "Ambient loops");
OPAAX_REGISTER_TAG("Audio.Bus.UI", "The UI's sounds, which a paused world does not silence");
