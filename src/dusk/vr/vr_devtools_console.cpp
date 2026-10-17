// vr_devtools_console.cpp -- see vr_devtools_console.hpp.

#if DUSK_VR_DEVTOOLS

#include "dusk/vr/vr_devtools_console.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "d/d_particle.h"
#include "d/d_stage.h"
#include "dusk/commands.hpp"
#include "dusk/game_mode.hpp"
#include "dusk/mods/svc/save.hpp"
#include "dusk/ui/ui.hpp"
#include "dusk/vr/vr_debug_log.hpp"
#include "dusk/vr/vr_devtools.hpp"
#include "SSystem/SComponent/c_math.h"
#include "f_op/f_op_actor_mng.h"
#include "f_op/f_op_actor_tag.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_manager.h"
#include "m_Do/m_Do_MemCard.h"
#include "m_Do/m_Do_MemCardRWmng.h"

#include <aurora/gfx.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <deque>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace dusk::vr::devtools {

namespace {

constexpr int kTicksPerSecond = 30;
constexpr int kReadyTimeoutTicks = 60 * kTicksPerSecond;
// `wait ready` also waits for this many settled ticks, so the fade-in is over.
constexpr int kReadySettleTicks = kTicksPerSecond;
// A screenshot needs the VR view to be rendering; give up after this long.
constexpr int kShotTimeoutTicks = 5 * kTicksPerSecond;
// `wait settled` (spec 13, item 1a): the actor count must hold this long, and
// gives up after the timeout.
constexpr int kSettledHoldTicks = 3 * kTicksPerSecond;
constexpr int kSettledTimeoutTicks = 30 * kTicksPerSecond;
// The only slot the save commands write to or load from (slot 1 is Daniel's
// playthrough; card slot indices are 0-based).
constexpr int kTestSlot = 2;

struct ButtonName {
    const char* name;
    u16 bit;
};
constexpr std::array<ButtonName, 12> kButtons{{
    {"a", PAD_BUTTON_A},       {"b", PAD_BUTTON_B},        {"x", PAD_BUTTON_X},
    {"y", PAD_BUTTON_Y},       {"z", PAD_TRIGGER_Z},       {"l", PAD_TRIGGER_L},
    {"r", PAD_TRIGGER_R},      {"start", PAD_BUTTON_START}, {"up", PAD_BUTTON_UP},
    {"down", PAD_BUTTON_DOWN}, {"left", PAD_BUTTON_LEFT},  {"right", PAD_BUTTON_RIGHT},
}};

struct ScriptedInput {
    float stickX = 0.f;
    float stickY = 0.f;
    int stickTicks = 0;
    std::array<int, kButtons.size()> buttonTicks{};
    float pendingTurnRad = 0.f;
    bool headLocked = false;
};

ScriptedInput& input() {
    static ScriptedInput s;
    return s;
}

struct Request {
    std::filesystem::path outPath;
    std::vector<std::string> lines;
    size_t next = 0;
    std::string scenario;
    std::string out;
    int waitTicks = 0;
    bool waitReady = false;
    int readyTicks = 0;
    int readyElapsed = 0;
    bool waitShot = false;
    int shotElapsed = 0;
    bool waitSettled = false;
    int settledCount = -1;
    int settledTicks = 0;
    int settledElapsed = 0;
};

// Perf log switch and pending segment mark (spec 13, items 1b and 5), read by
// vr_main.cpp's frame-end perf code.
struct PerfState {
    bool logOn = false;
    std::string pendingMark;
};
PerfState& perf() {
    static PerfState s;
    return s;
}

std::deque<Request>& queue() {
    static std::deque<Request> q;
    return q;
}

std::filesystem::path commandDir() {
    static std::filesystem::path dir = [] {
        std::filesystem::path p = baseDir() / "cmd";
        std::error_code ec;
        std::filesystem::create_directories(p, ec);
        return p;
    }();
    return dir;
}

std::optional<float> parseFloat(const std::string& s) {
    float v = 0.f;
    const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{} || ptr != s.data() + s.size() || !std::isfinite(v)) {
        return std::nullopt;
    }
    return v;
}

int secondsToTicks(float seconds) {
    return std::max(0, static_cast<int>(std::lround(seconds * kTicksPerSecond)));
}

void emit(Request& req, const std::string& text) {
    const std::string msg = "[devtools] " + text + "\n";
    duskVrLog(msg.c_str());
    req.out += text;
    req.out += '\n';
}

void loadRequest(const std::filesystem::path& path) {
    Request req;
    {
        std::ifstream in(path);
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            const auto first = line.find_first_not_of(" \t");
            if (first != std::string::npos && line[first] != '#') {
                req.lines.push_back(line.substr(first));
            }
        }
    }
    std::error_code ec;
    std::filesystem::remove(path, ec);
    req.outPath = path;
    req.outPath.replace_extension(".out");
    queue().push_back(std::move(req));
}

void finishRequest(Request& req) {
    std::filesystem::path tmp = req.outPath;
    tmp += ".tmp";
    {
        std::ofstream file(tmp, std::ios::binary);
        file << req.out;
    }
    std::error_code ec;
    std::filesystem::rename(tmp, req.outPath, ec);
}

// `crash`: crash the game on purpose, to check tools/crash.sh (spec 10, item
// 10). Kept out of line so the stack names it.
[[gnu::noinline]] void crashOnPurpose() {
    duskVrLog("[devtools] crash: crashing on purpose (null pointer write)\n");
    volatile int* p = nullptr;
    *p = 1;
}

bool isWorldReady() {
    return !dComIfGp_isEnableNextStage() && dComIfGp_getPlayer(0) != nullptr &&
           !dComIfGp_event_runCheck();
}

int loadedActorCount() {
    return g_fopAcTg_Queue.mSize;
}

// --- Save commands (spec 13, item 1a) ---------------------------------------
// The memory card runs on its own thread; these block the game for the
// fraction of a second a card read or write takes, which is fine for a
// scripted benchmark started from the title screen.
using SaveClock = std::chrono::steady_clock;

bool waitCardIdle() {
    const SaveClock::time_point deadline = SaveClock::now() + std::chrono::seconds(5);
    while (!mDoMemCd_isCardCommNone()) {
        if (SaveClock::now() >= deadline) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return true;
}

// Reads the card file's three quest logs into buf (SAVEFILE_SIZE bytes).
// `why` collects what happened on each attempt, for the error message.
bool readCard(u8* buf, std::string& why) {
    // At startup the launch menu (dusk/ui/prelaunch.cpp) sits over the title
    // and starts the memory card thread only when it closes; benchmarks run
    // from that menu, so start it the same way if it isn't running yet.
    if (!g_mDoMemCd_control.mInitialized) {
        mDoMemCd_ThdInit();
        why += " started the card thread;";
    }
    // Load() only takes the request if it gets the card mutex; a taken
    // request makes LoadSync() report "pending" (0) until the read is done.
    // An immediate failure (2) may be a request that wasn't taken (stale
    // state from an earlier operation): try again.
    for (int attempt = 0; attempt < 5; ++attempt) {
        if (!waitCardIdle()) {
            why += " card busy;";
            return false;
        }
        mDoMemCd_Load();
        u32 status = mDoMemCd_LoadSync(buf, SAVEFILE_SIZE, 0);
        why += " try" + std::to_string(attempt + 1) + " first=" + std::to_string(status);
        if (status == 1) {
            return true;
        }
        if (status == 2) {
            why += " card state " + std::to_string(mDoMemCd_getStatus(0)) + ";";
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }
        const SaveClock::time_point deadline = SaveClock::now() + std::chrono::seconds(5);
        while (SaveClock::now() < deadline) {
            status = mDoMemCd_LoadSync(buf, SAVEFILE_SIZE, 0);
            if (status != 0) {
                why += " then=" + std::to_string(status) + " card state " +
                       std::to_string(mDoMemCd_getStatus(0)) + ";";
                return status == 1;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        why += " timed out;";
        return false;
    }
    return false;
}

// `save copy 1 3`: the file select's Copy (dFile_select_c's copy command):
// copy the quest log, set its checksum, write the card file.
void saveCopy(Request& req) {
    static u8 buf[SAVEFILE_SIZE];
    std::string why;
    if (!readCard(buf, why)) {
        emit(req, "save copy: couldn't read the memory card:" + why);
        return;
    }
    if (!mDoMemCdRWm_TestCheckSumGameData(buf)) {
        emit(req, "save copy: slot 1 fails its checksum; nothing copied");
        return;
    }
    if (!waitCardIdle()) {
        emit(req, "save copy: the memory card is busy");
        return;
    }
    memcpy(buf + kTestSlot * SAVEDATA_SIZE, buf, SAVEDATA_SIZE);
    mDoMemCdRWm_SetCheckSumGameData(buf, kTestSlot);
    mDoMemCd_setCopyToPos(kTestSlot);
    mDoMemCd_save(buf, SAVEFILE_SIZE, 0);
    const SaveClock::time_point deadline = SaveClock::now() + std::chrono::seconds(10);
    while (SaveClock::now() < deadline) {
        const s32 status = mDoMemCd_SaveSync();
        if (status == 1) {
            emit(req, "save copy: slot 1 copied to slot 3");
            return;
        }
        if (status == 2) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    emit(req, "save copy: the card write failed");
}

// `save load 3`: what choosing slot 3 on the file select (or the command
// line's save option, d_s_logo.cpp) does, minus the scene change: the script
// warps next.
// `form`: "human"/"wolf" sets the loaded save's form before the warp, so Link
// arrives in it (Midna won't transform where people can see); empty keeps it.
// Until free transformation is unlocked (event bit 0x0D04), the game forces
// the wolf while any province is under twilight (dComIfGs_Wolf_Change_Check),
// so "human" also sets that bit, in memory only (nothing is saved). Twilit
// areas still can't hold human Link; benchmark those as saved (spec 13,
// Decision 6).
void saveLoad(Request& req, const std::string& form) {
    static u8 buf[SAVEFILE_SIZE];
    std::string why;
    if (!readCard(buf, why)) {
        emit(req, "save load: couldn't read the memory card:" + why);
        return;
    }
    if (!mDoMemCdRWm_TestCheckSumGameData(buf + kTestSlot * SAVEDATA_SIZE)) {
        emit(req, "save load: slot 3 is empty or fails its checksum (run tools/bench.sh copy-save)");
        return;
    }
    dComIfGs_setCardToMemory(buf, kTestSlot);
    // Loading at the title, unlike the file select: the title is a play
    // stage, and leaving it (the warp) writes its region memory back into
    // the save under its save-table number. Reload that region from the
    // save just loaded, so the write-back keeps the save's own bytes.
    if (stage_stag_info_class* stag = dComIfGp_getStageStagInfo()) {
        dComIfGs_getSave(dStage_stagInfo_GetSaveTbl(stag));
    }
    dComIfGs_setNoFile(kTestSlot + 1);
    dComIfGs_setDataNum(kTestSlot);
    dusk::mods::svc::save_slot_loaded(kTestSlot);
    if (const dusk::gamemode::GameMode* gameMode = dusk::gamemode::getGameModeManager().getCurrentGameMode()) {
        gameMode->invokeOnSaveLoadedFunction();
    }
    dKy_clear_game_init();
    dComIfGs_resetDan();
    dComIfGs_setRestartRoomParam(0);
    if (form == "human" || form == "wolf") {
        dComIfGs_setTransformStatus(form == "human" ? TF_STATUS_HUMAN : TF_STATUS_WOLF);
        if (form == "human") {
            dComIfGs_onEventBit(0x0D04);  // free transformation unlocked
        }
    }
    emit(req, "save load: slot 3 loaded" + (form.empty() ? std::string() : " as " + form));
}

void runSave(Request& req, const std::vector<std::string>& args) {
    if (args.size() == 4 && args[1] == "copy" && args[2] == "1" && args[3] == "3") {
        saveCopy(req);
    } else if ((args.size() == 3 || args.size() == 4) && args[1] == "load" && args[2] == "3" &&
               (args.size() == 3 || args[3] == "human" || args[3] == "wolf")) {
        saveLoad(req, args.size() == 4 ? args[3] : std::string());
    } else {
        emit(req, "Usage: save copy 1 3 | save load 3 [human|wolf] (slot 3 is the only test slot)");
    }
}

// --- Synthetic crowd (spec 13, item 1d) --------------------------------------
// `crowd <n>`: n of Castle Town's townsfolk (NPC_CD3) in rings around Link,
// the same every run: rings 120 units apart starting at 250, people ~120
// units apart along each ring, looks cycling through all 30 (the actor's
// "argument"), each facing Link, standing still (no path) and out all day.
void spawnCrowd(Request& req, int count) {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (player == nullptr) {
        emit(req, "crowd: no player");
        return;
    }
    constexpr int kLooks = 30;              // MAN_a .. WAN_b2 in d_stage.cpp's name table
    constexpr u32 kStandAllDay = 0xFFFFFF00;  // path none, start/end time none, group 0
    constexpr float kFirstRing = 250.f, kRingGap = 120.f, kSpacing = 120.f;
    const cXyz centre = player->current.pos;
    const int room = fopAcM_GetRoomNo(player);
    layer_class* savedLayer = fpcLy_CurrentLayer();
    if (base_process_class* playScene = fpcM_SearchByName(fpcNm_PLAY_SCENE_e)) {
        fpcLy_SetCurrentLayer(&((process_node_class*)playScene)->layer);
    }
    int spawned = 0, failed = 0, placed = 0;
    for (float radius = kFirstRing; placed < count; radius += kRingGap) {
        const int onRing = std::max(1, static_cast<int>(2.f * 3.14159265f * radius / kSpacing));
        for (int k = 0; k < onRing && placed < count; ++k, ++placed) {
            const float a = 2.f * 3.14159265f * k / onRing;
            cXyz pos(centre.x + radius * std::sin(a), centre.y + 50.f, centre.z + radius * std::cos(a));
            csXyz angle(0, cM_atan2s(centre.x - pos.x, centre.z - pos.z), 0);
            cXyz scale(1.f, 1.f, 1.f);
            const fpc_ProcID id = fopAcM_create(fpcNm_NPC_CD3_e, kStandAllDay, &pos, room, &angle, &scale,
                                                static_cast<s8>(placed % kLooks));
            (id != fpcM_ERROR_PROCESS_ID_e && id != 0 ? spawned : failed)++;
        }
    }
    fpcLy_SetCurrentLayer(savedLayer);
    emit(req, "crowd: spawned " + std::to_string(spawned) + (failed ? ", failed " + std::to_string(failed) : ""));
}

// --- Perf commands (spec 13, items 1b and 5) ---------------------------------
void runPerf(Request& req, const std::vector<std::string>& args) {
    if (args.size() == 3 && args[1] == "log" && (args[2] == "on" || args[2] == "off")) {
        perf().logOn = args[2] == "on";
        // Aurora's leftover diagnostics ([passdump], per-pass draws in
        // [gpuprof]) follow the same switch.
        aurora::gfx::set_perf_diagnostics(perf().logOn);
        emit(req, "perf log " + args[2]);
        return;
    }
    if (args.size() == 3 && args[1] == "mark") {
        // Logged by the frame-end perf code, after it closes the current
        // window, so no summary line spans two segments.
        perf().pendingMark = args[2];
        emit(req, "perf mark " + args[2]);
        return;
    }
    emit(req, "Usage: perf log on|off | perf mark <name>");
}

void runInput(Request& req, const std::vector<std::string>& args) {
    ScriptedInput& in = input();
    const std::string sub = args.size() > 1 ? args[1] : "";
    if (sub == "stick" && args.size() == 5) {
        const auto x = parseFloat(args[2]), y = parseFloat(args[3]), t = parseFloat(args[4]);
        if (!x || !y || !t) {
            emit(req, "Error: input stick <x> <y> <seconds>");
            return;
        }
        in.stickX = std::clamp(*x, -1.f, 1.f);
        in.stickY = std::clamp(*y, -1.f, 1.f);
        in.stickTicks = secondsToTicks(*t);
        return;
    }
    if (sub == "button" && args.size() == 4) {
        const auto t = parseFloat(args[3]);
        for (size_t i = 0; i < kButtons.size(); ++i) {
            if (args[2] == kButtons[i].name && t) {
                in.buttonTicks[i] = secondsToTicks(*t);
                return;
            }
        }
        emit(req, "Error: input button <a|b|x|y|z|l|r|start|up|down|left|right> <seconds>");
        return;
    }
    if (sub == "turn" && args.size() == 3) {
        const auto deg = parseFloat(args[2]);
        if (!deg) {
            emit(req, "Error: input turn <degrees>");
            return;
        }
        in.pendingTurnRad += *deg * 3.14159265f / 180.f;
        return;
    }
    if (sub == "stop" && args.size() == 2) {
        in.stickTicks = 0;
        in.buttonTicks.fill(0);
        return;
    }
    emit(req, "Usage: input stick <x> <y> <s> | input button <name> <s> | input turn <deg> | input stop");
}

// Runs lines until the script has to wait or ends. Returns true when done.
static bool s_remoteControl = false;

bool step(Request& req) {
    static CommandState s_state;  // one for the session, like the in-game console

    if (!s_remoteControl) {
        s_remoteControl = true;
        // Scripts warp straight from the title, where no file is loaded and
        // the file number defaults to slot 1. Point it at slot 3 so even a
        // stray save can't land in slot 1 (Daniel's playthrough).
        dComIfGs_setDataNum(2);
    }

    if (req.waitTicks > 0) {
        --req.waitTicks;
        return false;
    }
    if (req.waitReady) {
        req.readyTicks = isWorldReady() ? req.readyTicks + 1 : 0;
        if (req.readyTicks >= kReadySettleTicks) {
            req.waitReady = false;
        } else if (++req.readyElapsed >= kReadyTimeoutTicks) {
            emit(req, "wait ready: timed out after 60 s");
            req.waitReady = false;
        } else {
            return false;
        }
    }
    if (req.waitSettled) {
        const int count = loadedActorCount();
        if (count != req.settledCount) {
            req.settledCount = count;
            req.settledTicks = 0;
        }
        const bool held = ++req.settledTicks >= kSettledHoldTicks;
        const bool timedOut = ++req.settledElapsed >= kSettledTimeoutTicks;
        if (!held && !timedOut) {
            return false;
        }
        emit(req, "actors " + std::to_string(count) + (held ? " settled" : " (still changing after 30 s)"));
        req.waitSettled = false;
    }
    if (req.waitShot) {
        if (shotInProgress() && ++req.shotElapsed < kShotTimeoutTicks) {
            return false;
        }
        if (shotInProgress()) {
            emit(req, "screenshot: timed out (is the VR view rendering?)");
            shot().requested = false;
        }
        req.waitShot = false;
    }

    while (req.next < req.lines.size()) {
        const std::string& line = req.lines[req.next++];
        std::vector<std::string> args;
        {
            std::istringstream ss(line);
            std::string a;
            while (ss >> a) {
                args.push_back(a);
            }
        }
        const std::string& cmd = args[0];
        if (cmd == "wait") {
            if (args.size() == 2 && args[1] == "settled") {
                req.waitSettled = true;
                req.settledCount = -1;
                req.settledTicks = 0;
                req.settledElapsed = 0;
                return false;
            }
            if (args.size() == 2 && args[1] == "ready") {
                req.waitReady = true;
                req.readyTicks = 0;
                req.readyElapsed = 0;
                return false;
            }
            const auto t = args.size() == 2 ? parseFloat(args[1]) : std::nullopt;
            if (!t) {
                emit(req, "Usage: wait <seconds> | wait ready | wait settled");
                continue;
            }
            req.waitTicks = secondsToTicks(*t);
            return false;
        }
        if (cmd == "screenshot") {
            const std::string name = args.size() > 1 ? args[1] : "shot";
            requestShot(req.scenario.empty() ? name : req.scenario + "-" + name);
            emit(req, "screenshot " + shot().name);
            req.waitShot = true;
            req.shotElapsed = 0;
            return false;
        }
        if (cmd == "scenario" && args.size() == 2) {
            req.scenario = args[1];
            continue;
        }
        if (cmd == "input") {
            runInput(req, args);
            continue;
        }
        if (cmd == "save") {
            runSave(req, args);
            continue;
        }
        if (cmd == "perf") {
            runPerf(req, args);
            continue;
        }
        if (cmd == "crowd" && args.size() == 2) {
            const auto n = parseFloat(args[1]);
            if (!n || *n < 0 || *n > 300) {
                emit(req, "Usage: crowd <0-300>");
            } else {
                spawnCrowd(req, static_cast<int>(*n));
            }
            continue;
        }
        if (cmd == "actors" && args.size() == 1) {
            emit(req, "actors " + std::to_string(loadedActorCount()));
            continue;
        }
        if (cmd == "expect" && args.size() == 3 && args[1] == "actors") {
            // tools/bench.sh marks the run invalid on "INVALID".
            const auto min = parseFloat(args[2]);
            const int count = loadedActorCount();
            if (!min) {
                emit(req, "Usage: expect actors <minimum>");
            } else if (count >= *min) {
                emit(req, "expect actors " + std::to_string(count) + " >= " + args[2] + " ok");
            } else {
                emit(req, "expect actors " + std::to_string(count) + " < " + args[2] + " INVALID");
            }
            continue;
        }
        if (cmd == "menu" && args.size() == 2 && args[1] == "close") {
            // Menus block the game's controller input; warping from the title
            // screen leaves its menu open.
            dusk::ui::close_all_documents();
            continue;
        }
        if (cmd == "metrics" && args.size() == 2 && (args[1] == "start" || args[1] == "stop")) {
            // The game can't drive the metrics app; this marks the time
            // (device epoch ms) and scenario.sh starts/stops the recording
            // around the script and summarises just this window.
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            emit(req, "metrics " + args[1] + " " + std::to_string(ms));
            continue;
        }
        if (cmd == "crash" && args.size() == 1) {
            crashOnPurpose();
        }
        if (cmd == "head" && args.size() == 2 && (args[1] == "lock" || args[1] == "unlock")) {
            input().headLocked = args[1] == "lock";
            continue;
        }
        runCommand(line, s_state, [&req](std::string text) { emit(req, text); });
    }
    return true;
}

void pollRequests() {
    using Clock = std::chrono::steady_clock;
    static Clock::time_point s_next{};
    const Clock::time_point now = Clock::now();
    if (now < s_next) {
        return;
    }
    s_next = now + std::chrono::milliseconds(250);

    std::vector<std::filesystem::path> requests;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(commandDir(), ec)) {
        if (entry.path().extension() == ".req") {
            requests.push_back(entry.path());
        }
    }
    // Ids start with a timestamp, so name order is arrival order.
    std::sort(requests.begin(), requests.end());
    for (const auto& path : requests) {
        loadRequest(path);
    }
}

}  // namespace

void processRemoteCommands() {
    ScriptedInput& in = input();
    if (in.stickTicks > 0) {
        --in.stickTicks;
    }
    for (int& t : in.buttonTicks) {
        if (t > 0) {
            --t;
        }
    }

    pollRequests();
    // Finished requests (no waits) complete in this tick; a waiting script
    // holds the rest of the queue back.
    while (!queue().empty() && step(queue().front())) {
        finishRequest(queue().front());
        queue().pop_front();
    }
}

void applyScriptedInput(PADStatus& pad) {
    const ScriptedInput& in = input();
    if (in.stickTicks > 0) {
        pad.stickX = static_cast<s8>(in.stickX * 127.f);
        pad.stickY = static_cast<s8>(in.stickY * 127.f);
    }
    for (size_t i = 0; i < kButtons.size(); ++i) {
        if (in.buttonTicks[i] > 0) {
            pad.button |= kButtons[i].bit;
            if (kButtons[i].bit == PAD_TRIGGER_L) {
                pad.triggerLeft = 255;
            } else if (kButtons[i].bit == PAD_TRIGGER_R) {
                pad.triggerRight = 255;
            }
        }
    }
}

float takeScriptedTurnRad() {
    const float turn = input().pendingTurnRad;
    input().pendingTurnRad = 0.f;
    return turn;
}

bool isHeadLocked() {
    return input().headLocked;
}

bool perfLogOn() {
    return perf().logOn;
}

bool takePerfMark(std::string& name) {
    if (perf().pendingMark.empty()) {
        return false;
    }
    name = std::move(perf().pendingMark);
    perf().pendingMark.clear();
    return true;
}

int liveParticleCount() {
    JPAEmitterManager* mgr = dPa_control_c::getEmitterManager();
    return mgr != nullptr ? mgr->getParticleNumber() : 0;
}

bool hasRemoteControl() {
    return s_remoteControl;
}

}  // namespace dusk::vr::devtools

#endif  // DUSK_VR_DEVTOOLS
