// vr_devtools_console.hpp -- remote console for the VR build (spec
// docs/vr-specs/10-dev-tooling.md, item 4). Compiled only with
// -DDUSK_VR_DEVTOOLS=ON; normal builds contain none of this.
//
// The PC runs Dusklight console commands in the running game by dropping
// request files into the game's app storage (tools/cmd.sh):
//   devtools/cmd/<id>.req   one command per line (blank and '#' lines skipped)
//   devtools/cmd/<id>.out   the commands' output (written to .tmp, then renamed)
// Output also goes to the log, prefixed "[devtools] ".

#pragma once

#if DUSK_VR_DEVTOOLS

namespace dusk::vr::devtools {

// Once per sim tick, from the game loop, before the game logic runs.
// Checks for requests about every 250 ms and runs them there, so commands
// act on the game between ticks, never in the middle of a draw.
void processRemoteCommands();

}  // namespace dusk::vr::devtools

#endif  // DUSK_VR_DEVTOOLS
