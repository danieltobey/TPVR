// vr_devtools_console.cpp -- see vr_devtools_console.hpp.

#if DUSK_VR_DEVTOOLS

#include "dusk/vr/vr_devtools_console.hpp"

#include "d/d_com_inf_game.h"
#include "dusk/commands.hpp"
#include "dusk/ui/ui.hpp"
#include "dusk/vr/vr_debug_log.hpp"
#include "dusk/vr/vr_devtools.hpp"

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
#include <vector>

namespace dusk::vr::devtools {

namespace {

constexpr int kTicksPerSecond = 30;
constexpr int kReadyTimeoutTicks = 60 * kTicksPerSecond;
// `wait ready` also waits for this many settled ticks, so the fade-in is over.
constexpr int kReadySettleTicks = kTicksPerSecond;
// A screenshot needs the VR view to be rendering; give up after this long.
constexpr int kShotTimeoutTicks = 5 * kTicksPerSecond;

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
};

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

bool isWorldReady() {
    return !dComIfGp_isEnableNextStage() && dComIfGp_getPlayer(0) != nullptr &&
           !dComIfGp_event_runCheck();
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
bool step(Request& req) {
    static CommandState s_state;  // one for the session, like the in-game console

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
            if (args.size() == 2 && args[1] == "ready") {
                req.waitReady = true;
                req.readyTicks = 0;
                req.readyElapsed = 0;
                return false;
            }
            const auto t = args.size() == 2 ? parseFloat(args[1]) : std::nullopt;
            if (!t) {
                emit(req, "Usage: wait <seconds> | wait ready");
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
        if (cmd == "menu" && args.size() == 2 && args[1] == "close") {
            // Menus block the game's controller input; warping from the title
            // screen leaves its menu open.
            dusk::ui::close_all_documents();
            continue;
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

}  // namespace dusk::vr::devtools

#endif  // DUSK_VR_DEVTOOLS
