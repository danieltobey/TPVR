// vr_devtools_console.hpp -- remote console and test driver for the VR build
// (spec docs/vr-specs/10-dev-tooling.md, items 4 and 5). Compiled only with
// -DDUSK_VR_DEVTOOLS=ON; normal builds contain none of this.
//
// The PC runs Dusklight console commands in the running game by dropping
// request files into the game's app storage (tools/cmd.sh, tools/scenario.sh):
//   devtools/cmd/<id>.req   one command per line (blank and '#' lines skipped)
//   devtools/cmd/<id>.out   the commands' output (written to .tmp, then renamed)
// Output also goes to the log, prefixed "[devtools] ".
//
// A request is a small script: besides console commands it can use
//   wait <seconds> | wait ready     pause the script (game ticks / until loaded)
//   screenshot <name>               capture the next frame, wait until saved
//   scenario <name>                 prefix this request's screenshot names
//   input stick <x> <y> <seconds>   hold the left stick (-1..1)
//   input button <name> <seconds>   hold a button (a b x y z l r start up down left right)
//   input turn <degrees>            turn the view (positive = right)
//   input stop                      release all scripted input
//   head lock | head unlock         fixed head pose for repeatable views
//   menu close                      close open menus (they block game input)
//   metrics start | metrics stop    mark a measurement window (scenario.sh records it)
//   crash                           crash the game on purpose (tests tools/crash.sh)
// Requests run one at a time, in arrival order.

#pragma once

#if DUSK_VR_DEVTOOLS

#include <dolphin/pad.h>

namespace dusk::vr::devtools {

// Once per sim tick, from the game loop, before the game logic runs.
// Checks for requests about every 250 ms and runs them there, so commands
// act on the game between ticks, never in the middle of a draw. Also counts
// down held scripted input.
void processRemoteCommands();

// VR input (vr_main.cpp), each frame, just before the pad status goes to the
// game: apply held scripted stick/buttons.
void applyScriptedInput(PADStatus& pad);

// VR input, each frame: view turn requested by `input turn` since the last
// call, in radians (positive = right).
float takeScriptedTurnRad();

// `head lock`: render from a fixed head pose instead of the tracked one.
bool isHeadLocked();

// True once any remote command has run this session. Auto-save is off from
// then on (dusk/autosave.cpp), so scripted runs never write Daniel's saves;
// slot 1 is his playthrough and must never be overwritten.
bool hasRemoteControl();

}  // namespace dusk::vr::devtools

#endif  // DUSK_VR_DEVTOOLS
