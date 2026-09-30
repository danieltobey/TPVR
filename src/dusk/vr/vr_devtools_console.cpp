// vr_devtools_console.cpp -- see vr_devtools_console.hpp.

#if DUSK_VR_DEVTOOLS

#include "dusk/vr/vr_devtools_console.hpp"

#include "dusk/commands.hpp"
#include "dusk/vr/vr_devtools.hpp"
#include "dusk/vr/vr_debug_log.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace dusk::vr::devtools {

namespace {

std::filesystem::path commandDir() {
    static std::filesystem::path dir = [] {
        std::filesystem::path p = baseDir() / "cmd";
        std::error_code ec;
        std::filesystem::create_directories(p, ec);
        return p;
    }();
    return dir;
}

void runRequest(const std::filesystem::path& req) {
    // One state for the whole session, like the in-game console, so @found
    // and history carry over between requests.
    static CommandState s_state;

    std::vector<std::string> lines;
    {
        std::ifstream in(req);
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (!line.empty() && line[0] != '#') {
                lines.push_back(std::move(line));
            }
        }
    }
    std::error_code ec;
    std::filesystem::remove(req, ec);

    std::string out;
    const CommandOutput output = [&out](std::string text) {
        std::string msg = "[devtools] " + text + "\n";
        duskVrLog(msg.c_str());
        out += text;
        out += '\n';
    };
    for (const std::string& line : lines) {
        runCommand(line, s_state, output);
    }

    std::filesystem::path result = req;
    result.replace_extension(".out");
    std::filesystem::path tmp = result;
    tmp += ".tmp";
    {
        std::ofstream file(tmp, std::ios::binary);
        file << out;
    }
    std::filesystem::rename(tmp, result, ec);
}

}  // namespace

void processRemoteCommands() {
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
    for (const auto& req : requests) {
        runRequest(req);
    }
}

}  // namespace dusk::vr::devtools

#endif  // DUSK_VR_DEVTOOLS
