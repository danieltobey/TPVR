// vr_devtools.hpp -- developer tooling for the VR build (spec
// docs/vr-specs/10-dev-tooling.md). Compiled only with -DDUSK_VR_DEVTOOLS=ON;
// normal builds contain none of this.
//
// Screenshot (spec item 3): the PC asks for a capture by writing a request
// file into the game's app storage (tools/screenshot.sh); the game polls for
// it about once a second, copies the next rendered stereo image (both eyes,
// side by side) to a readback buffer, and after the frame is submitted writes
// it as a PNG. It captures exactly what the renderer drew, independent of the
// system compositor or whether the headset is being worn.
//
// Files live in <app storage>/devtools/ -- on Android the app-specific
// external files dir (readable with plain `adb pull`), elsewhere ./devtools/.
//   devtools/screenshot.req   request; optional contents = file name stem
//   devtools/shots/<name>.png result (written to .tmp, then renamed)

#pragma once

#if DUSK_VR_DEVTOOLS

#include <aurora/gfx.hpp>
#include "../../../extern/aurora/lib/webgpu/gpu.hpp"
#include "dusk/vr/vr_debug_log.hpp"

#include <png.h>

#if defined(__ANDROID__)
#include <SDL3/SDL_system.h>
#endif

#include <cctype>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace dusk::vr::devtools {

inline std::filesystem::path baseDir() {
    static std::filesystem::path dir = [] {
        std::filesystem::path p = "devtools";
#if defined(__ANDROID__)
        if (const char* ext = SDL_GetAndroidExternalStoragePath()) {
            p = std::filesystem::path(ext) / "devtools";
        }
#endif
        std::error_code ec;
        std::filesystem::create_directories(p / "shots", ec);
        return p;
    }();
    return dir;
}

struct ShotState {
    bool requested = false;
    bool pending = false;
    std::string name;
    wgpu::Texture src;
    wgpu::Buffer buffer;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t bytesPerRow = 0;
    wgpu::TextureFormat format = wgpu::TextureFormat::Undefined;
    aurora::gfx::EncoderTaskId taskId = aurora::gfx::InvalidEncoderTask;
};

inline ShotState& shot() {
    static ShotState s;
    return s;
}

inline void copyTaskCallback(const aurora::gfx::EncoderTaskContext& /*ctx*/,
                             const wgpu::CommandEncoder& cmd, const void* /*payload*/,
                             size_t /*payloadSize*/, void* /*userdata*/) {
    ShotState& s = shot();
    if (!s.src || !s.buffer) {
        return;
    }
    wgpu::CommandEncoder mutableCmd = cmd;  // CopyTextureToBuffer is non-const

    wgpu::TexelCopyTextureInfo srcCopy{};
    srcCopy.texture = s.src;
    srcCopy.aspect = wgpu::TextureAspect::All;

    wgpu::TexelCopyBufferInfo dstCopy{};
    dstCopy.buffer = s.buffer;
    dstCopy.layout.offset = 0;
    dstCopy.layout.bytesPerRow = s.bytesPerRow;
    dstCopy.layout.rowsPerImage = s.height;

    wgpu::Extent3D extent{s.width, s.height, 1};
    mutableCmd.CopyTextureToBuffer(&srcCopy, &dstCopy, &extent);
}

// Once at VR session startup, before the first frame.
inline void registerTasks() {
    aurora::gfx::EncoderTaskDescriptor desc{
        .label = "vr_devtools_screenshot",
        .callback = &copyTaskCallback,
        .userdata = nullptr,
    };
    shot().taskId = aurora::gfx::register_encoder_task_type(desc);
}

// Ask for the next rendered frame to be saved as shots/<name>.png. The name
// is reduced to a safe file stem.
inline void requestShot(const std::string& name) {
    std::string clean;
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_') {
            clean += c;
        }
    }
    if (clean.empty()) {
        clean = "shot";
    }
    shot().name = clean;
    shot().requested = true;
}

// True from requestShot() until the PNG has been written.
inline bool shotInProgress() {
    return shot().requested || shot().pending;
}

// Once per frame. Checks the request file at most once a second.
inline void pollTriggers() {
    using Clock = std::chrono::steady_clock;
    static Clock::time_point s_next{};
    const Clock::time_point now = Clock::now();
    if (now < s_next) {
        return;
    }
    s_next = now + std::chrono::seconds(1);

    const std::filesystem::path req = baseDir() / "screenshot.req";
    std::error_code ec;
    if (!std::filesystem::exists(req, ec)) {
        return;
    }
    std::string name;
    {
        std::ifstream in(req);
        std::getline(in, name);
    }
    std::filesystem::remove(req, ec);
    requestShot(name);
}

// Call right after the stereo/eye pass has ended (back on the EFB pass, where
// push_encoder_task is allowed) with the image that was just rendered.
inline void captureIfRequested(const wgpu::Texture& tex, uint32_t width, uint32_t height,
                               wgpu::TextureFormat format) {
    ShotState& s = shot();
    if (!s.requested || s.pending || !tex || width == 0 || height == 0 ||
        s.taskId == aurora::gfx::InvalidEncoderTask) {
        return;
    }
    s.requested = false;
    s.src = tex;
    s.width = width;
    s.height = height;
    s.format = format;
    s.bytesPerRow = (width * 4 + 255) & ~255u;

    wgpu::BufferDescriptor desc{};
    desc.size = static_cast<uint64_t>(s.bytesPerRow) * height;
    desc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
    s.buffer = aurora::webgpu::g_device.CreateBuffer(&desc);

    const uint8_t payload = 0;  // the task reads its state from shot(), not the payload
    aurora::gfx::push_encoder_task(s.taskId, &payload, sizeof(payload));
    s.pending = true;
}

// Call after aurora::gfx::synchronize() in submitFrame(): the copy has run.
inline void finishIfPending() {
    ShotState& s = shot();
    if (!s.pending) {
        return;
    }
    s.pending = false;

    bool swapRB = false;
    switch (s.format) {
    case wgpu::TextureFormat::RGBA8Unorm:
    case wgpu::TextureFormat::RGBA8UnormSrgb:
        break;
    case wgpu::TextureFormat::BGRA8Unorm:
    case wgpu::TextureFormat::BGRA8UnormSrgb:
        swapRB = true;
        break;
    default: {
        char msg[128];
        duskVrSnprintf(msg, sizeof(msg), "[devtools] screenshot: unsupported format %d\n",
                       static_cast<int>(s.format));
        duskVrLog(msg);
        s.src = nullptr;
        s.buffer = nullptr;
        return;
    }
    }

    const size_t size = static_cast<size_t>(s.bytesPerRow) * s.height;
    bool done = false;
    bool ok = false;
    const auto future = s.buffer.MapAsync(
        wgpu::MapMode::Read, 0, size, wgpu::CallbackMode::WaitAnyOnly,
        [&done, &ok](wgpu::MapAsyncStatus status, wgpu::StringView /*message*/) {
            done = true;
            ok = (status == wgpu::MapAsyncStatus::Success);
        });
    aurora::webgpu::g_instance.WaitAny(future, 5000000000);
    if (!done || !ok) {
        duskVrLog("[devtools] screenshot: MapAsync failed\n");
        s.src = nullptr;
        s.buffer = nullptr;
        return;
    }

    // Tight RGBA copy with opaque alpha (the eye image's alpha isn't meaningful).
    const auto* src = static_cast<const uint8_t*>(s.buffer.GetConstMappedRange(0, size));
    std::vector<uint8_t> rgba(static_cast<size_t>(s.width) * s.height * 4);
    for (uint32_t y = 0; y < s.height; ++y) {
        const uint8_t* row = src + static_cast<size_t>(y) * s.bytesPerRow;
        uint8_t* out = rgba.data() + static_cast<size_t>(y) * s.width * 4;
        for (uint32_t x = 0; x < s.width; ++x) {
            out[x * 4 + 0] = row[x * 4 + (swapRB ? 2 : 0)];
            out[x * 4 + 1] = row[x * 4 + 1];
            out[x * 4 + 2] = row[x * 4 + (swapRB ? 0 : 2)];
            out[x * 4 + 3] = 255;
        }
    }
    s.buffer.Unmap();
    s.src = nullptr;
    s.buffer = nullptr;

    const std::filesystem::path dir = baseDir() / "shots";
    const std::filesystem::path tmp = dir / (s.name + ".png.tmp");
    const std::filesystem::path final = dir / (s.name + ".png");
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    image.width = s.width;
    image.height = s.height;
    image.format = PNG_FORMAT_RGBA;
    const bool wrote =
        png_image_write_to_file(&image, tmp.c_str(), 0, rgba.data(), 0, nullptr) != 0;
    std::error_code ec;
    if (wrote) {
        std::filesystem::rename(tmp, final, ec);
    }
    char msg[512];
    duskVrSnprintf(msg, sizeof(msg), "[devtools] screenshot %s: %s (%ux%u)\n",
                   wrote && !ec ? "saved" : "FAILED", final.c_str(), s.width, s.height);
    duskVrLog(msg);
}

}  // namespace dusk::vr::devtools

#endif  // DUSK_VR_DEVTOOLS
