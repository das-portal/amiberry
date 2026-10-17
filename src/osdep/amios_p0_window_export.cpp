/*
 * AmiOS Wayland P0 - Amiberry 8.4.1
 *
 * Read-only guest discovery and independent SDL3 window textures, with optional
 * experimental input forwarding. No changes to Intuition, layers.library or
 * Amiberry's native/RTG renderer. Designed for the non-overlapping guest
 * framebuffer arrangement created by the Stage 4 allocator.
 *
 * This is a test build, not a replacement for the historical RC1 input bridge.
 */
#include "sysdeps.h"
#include "amios_p0_window_export.h"
#include "amiberry_gfx.h"
#include "amiberry_input.h"
#include "inputdevice.h"
#include "memory.h"
#include "target.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {
constexpr uaecptr kExecLibList = 0x17a;
constexpr uaecptr kExecPortList = 392;
constexpr uaecptr kNodeNext = 0;
constexpr uaecptr kNodeName = 10;
constexpr uaecptr kFirstScreen = 0x3c;
constexpr uaecptr kScreenNext = 0;
constexpr uaecptr kScreenFirstWindow = 4;
constexpr uaecptr kScreenWidth = 12;
constexpr uaecptr kScreenHeight = 14;
constexpr uaecptr kWindowNext = 0;
constexpr uaecptr kWindowLeft = 4;
constexpr uaecptr kWindowTop = 6;
constexpr uaecptr kWindowWidth = 8;
constexpr uaecptr kWindowHeight = 10;
constexpr uaecptr kWindowMinW = 16, kWindowMinH = 18, kWindowMaxW = 20, kWindowMaxH = 22;
constexpr uaecptr kWindowFlags = 24;
constexpr uaecptr kWindowTitle = 32;
constexpr uaecptr kWindowBorderLeft = 54;
constexpr uaecptr kWindowBorderTop = 55;
constexpr uaecptr kWindowBorderRight = 56;
constexpr uaecptr kWindowBorderBottom = 57;
constexpr uae_u32 kSizeGadget = 1;
constexpr uae_u32 kBackdrop = 0x100;
constexpr uint64_t kResizeDebounce = 40, kResizeIdle = 110, kResizeActivation = 60;
constexpr uint64_t kResizeActivationWait = 120, kResizePressHold = 40;
constexpr uint64_t kResizeResultTimeout = 2500;
constexpr size_t kMaxWindows = 64;
constexpr int kRetireAfterMisses = 6;
// AmiOSBridge v0.10 is the unchanged Stage 4 protocol-2 allocator.
constexpr uaecptr kBridgeMagic = 36, kBridgeVersion = 40;
constexpr uaecptr kBridgeCommand = 44, kBridgeSequence = 48;
constexpr uaecptr kBridgeAck = 52, kBridgeWindow = 56, kBridgeResult = 60;
constexpr uae_u32 kBridgeMagicValue = 0x414d4934, kBridgeVersionValue = 2;
constexpr uae_u32 kBridgeClose = 1, kBridgePosted = 1;
constexpr uint64_t kBridgeTimeoutMs = 1500;

struct GuestWindow {
    uaecptr id = 0;
    int x = 0, y = 0, w = 0, h = 0;
    int outer_x = 0, outer_y = 0, outer_w = 0, outer_h = 0;
    int left_border = 0, top_border = 0, right_border = 0, bottom_border = 0;
    int min_w = 1, min_h = 1, max_w = 0, max_h = 0;
    bool resizable = false;
    std::string title;
};
enum class ClosePhase { idle, request, wait_ack, wait_guest };
enum class ResizePhase { idle, activation_press, activation_wait, press_hold, dragging, waiting_result };
struct ExportedWindow {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    SDL_PixelFormat texture_format = SDL_PIXELFORMAT_UNKNOWN;
    int width = 0, height = 0;
    int guest_x = 0, guest_y = 0;
    int missed = 0;
    bool seen = false;
    bool has_frame = false;
    bool resizable = false;
    int pending_w = 0, pending_h = 0;
    uint64_t pending_at = 0;
    int resize_start_w = 0, resize_start_h = 0;
    int start_x = 0, start_y = 0, target_x = 0, target_y = 0;
    bool synthetic_down = false;
    ResizePhase resize_phase = ResizePhase::idle;
    uint64_t resize_phase_at = 0, last_pointer_at = 0;
    int expected_host_w = 0, expected_host_h = 0;
    uint64_t expected_until = 0;
    ClosePhase close_phase = ClosePhase::idle;
    uae_u32 close_sequence = 0;
    uint64_t close_started = 0;
    std::string title;
};

std::map<uaecptr, ExportedWindow> windows;
bool initialized = false, enabled = false, input_enabled = false, resize_enabled = false;
bool hide_root = false, root_hidden = false, root_unhidden_by_user = false;
int monitor_id = 0;
uaecptr intuition_cache = 0, bridge_port_cache = 0;
uae_u32 bridge_next_sequence = 1;
uint64_t last_warning_ms = 0;

bool flag(const char* name) {
    const char* v = std::getenv(name);
    return v && (!SDL_strcasecmp(v, "1") || !SDL_strcasecmp(v, "yes") ||
                 !SDL_strcasecmp(v, "true") || !SDL_strcasecmp(v, "on"));
}
void init() {
    if (initialized) return;
    initialized = true;
    enabled = flag("AMIOS_EXPORT_WINDOWS");
    input_enabled = enabled && flag("AMIOS_EXPORT_INPUT");
    resize_enabled = input_enabled && flag("AMIOS_EXPORT_RESIZE");
    hide_root = enabled && flag("AMIOS_HIDE_ROOT");
    if (const char* s = std::getenv("AMIOS_RTG_MONITOR")) {
        char* end = nullptr;
        const long value = std::strtol(s, &end, 10);
        if (end && *end == '\0' && value >= 0 && value < MAX_AMIGAMONITORS)
            monitor_id = static_cast<int>(value);
    }
    if (enabled)
        write_log("AmiOS P0: SDL window export enabled, RTG monitor %d, input=%d, hide-root=%d\n",
                  monitor_id, input_enabled ? 1 : 0, hide_root ? 1 : 0);
}
bool readable(uaecptr addr, uae_u32 n) {
    return addr >= 0x100 && valid_address(addr, n);
}
std::string guest_string(uaecptr ptr, size_t limit = 100) {
    std::string out;
    if (!readable(ptr, 1)) return out;
    for (size_t i = 0; i < limit && readable(ptr + static_cast<uaecptr>(i), 1); ++i) {
        const char c = static_cast<char>(get_byte(ptr + static_cast<uaecptr>(i)));
        if (!c || (static_cast<unsigned char>(c) < 32 && c != '\t')) break;
        out += c;
    }
    return out;
}
uaecptr intuition() {
    if (readable(intuition_cache + kNodeName, 4) &&
        guest_string(get_long(intuition_cache + kNodeName)) == "intuition.library")
        return intuition_cache;
    intuition_cache = 0;
    if (!valid_address(4, 4)) return 0;
    const uaecptr exec = get_long(4);
    if (!readable(exec + kExecLibList, 12)) return 0;
    uaecptr node = get_long(exec + kExecLibList);
    for (int i = 0; i < 256 && readable(node, kNodeName + 4); ++i) {
        if (guest_string(get_long(node + kNodeName)) == "intuition.library") {
            intuition_cache = node;
            return node;
        }
        const uaecptr next = get_long(node + kNodeNext);
        if (!next || next == node) break;
        node = next;
    }
    return 0;
}
uaecptr rtg_screen(int width, int height) {
    const uaecptr base = intuition();
    if (!base || !readable(base + kFirstScreen, 4)) return 0;
    uaecptr screen = get_long(base + kFirstScreen);
    for (int i = 0; i < 32 && readable(screen, kScreenHeight + 2); ++i) {
        // Deliberately require exact guest Intuition geometry, not merely an
        // Amiberry surface that happens to have been allocated at this size.
        if (get_word(screen + kScreenWidth) == width &&
            get_word(screen + kScreenHeight) == height)
            return screen;
        const uaecptr next = get_long(screen + kScreenNext);
        if (!next || next == screen) break;
        screen = next;
    }
    return 0;
}

uaecptr bridge_port() {
    if (readable(bridge_port_cache + kBridgeResult, 4) &&
        get_long(bridge_port_cache + kBridgeMagic) == kBridgeMagicValue &&
        get_long(bridge_port_cache + kBridgeVersion) == kBridgeVersionValue)
        return bridge_port_cache;
    bridge_port_cache = 0;
    if (!valid_address(4, 4)) return 0;
    const uaecptr exec = get_long(4);
    if (!readable(exec + kExecPortList, 12)) return 0;
    uaecptr port = get_long(exec + kExecPortList);
    for (int n = 0; n < 256 && readable(port, kBridgeResult + 4); ++n) {
        if (guest_string(get_long(port + kNodeName)) == "AmiOS68k.Port" &&
            get_long(port + kBridgeMagic) == kBridgeMagicValue &&
            get_long(port + kBridgeVersion) == kBridgeVersionValue) {
            bridge_port_cache = port;
            return port;
        }
        const uaecptr next = get_long(port + kNodeNext);
        if (!next || next == port) break;
        port = next;
    }
    return 0;
}
void advance_close(ExportedWindow& slot, const uaecptr guest_id) {
    if (slot.close_phase == ClosePhase::idle) return;
    const uint64_t now = SDL_GetTicks();
    if (slot.close_phase == ClosePhase::request) {
        const uaecptr port = bridge_port();
        if (port && !get_long(port + kBridgeCommand)) {
            uae_u32 seq = bridge_next_sequence++;
            if (!seq) seq = bridge_next_sequence++;
            put_long(port + kBridgeWindow, guest_id);
            put_long(port + kBridgeResult, 0);
            put_long(port + kBridgeSequence, seq);
            put_long(port + kBridgeCommand, kBridgeClose); // command written last
            slot.close_sequence = seq;
            slot.close_started = now;
            slot.close_phase = ClosePhase::wait_ack;
            write_log("AmiOS P0: requested guest close Window* %08x seq=%u\n",
                      static_cast<unsigned>(guest_id), seq);
        } else if (now - slot.close_started >= kBridgeTimeoutMs) {
            write_log("AmiOS P0: close not sent (AmiOSBridge unavailable/busy); guest preserved\n");
            slot.close_phase = ClosePhase::idle;
        }
    } else if (slot.close_phase == ClosePhase::wait_ack) {
        const uaecptr port = bridge_port();
        if (port && get_long(port + kBridgeAck) == slot.close_sequence) {
            const uae_u32 result = get_long(port + kBridgeResult);
            if (result == kBridgePosted) {
                slot.close_phase = ClosePhase::wait_guest;
                slot.close_started = now;
            } else {
                // In protocol 2, result 3 means no close gadget.
                // Don't send Escape to an arbitrary requester or close the host view.
                write_log("AmiOS P0: guest cannot close this window (result=%u)\n", result);
                slot.close_phase = ClosePhase::idle;
            }
        } else if (now - slot.close_started >= kBridgeTimeoutMs) {
            write_log("AmiOS P0: guest close acknowledgement timeout; guest preserved\n");
            slot.close_phase = ClosePhase::idle;
        }
    } else if (slot.close_phase == ClosePhase::wait_guest &&
               now - slot.close_started >= kBridgeTimeoutMs) {
        write_log("AmiOS P0: guest remains open after close; waiting for application\n");
        slot.close_phase = ClosePhase::idle;
    }
}

std::vector<GuestWindow> scan(SDL_Surface* source) {
    std::vector<GuestWindow> found;
    const uaecptr screen = rtg_screen(source->w, source->h);
    if (!screen || !readable(screen, kScreenFirstWindow + 4))
        return found;
    uaecptr ptr = get_long(screen + kScreenFirstWindow);
    for (int i = 0; i < 512 && found.size() < kMaxWindows &&
         readable(ptr, kWindowBorderBottom + 1); ++i) {
        const uae_u32 flags = get_long(ptr + kWindowFlags);
        const int x = static_cast<int16_t>(get_word(ptr + kWindowLeft));
        const int y = static_cast<int16_t>(get_word(ptr + kWindowTop));
        const int outer_w = get_word(ptr + kWindowWidth);
        const int outer_h = get_word(ptr + kWindowHeight);
        const int l = get_byte(ptr + kWindowBorderLeft);
        const int t = get_byte(ptr + kWindowBorderTop);
        const int r = get_byte(ptr + kWindowBorderRight);
        const int b = get_byte(ptr + kWindowBorderBottom);
        GuestWindow w;
        w.id = ptr;
        w.outer_x = x; w.outer_y = y; w.outer_w = outer_w; w.outer_h = outer_h;
        w.left_border = l; w.top_border = t; w.right_border = r; w.bottom_border = b;
        w.min_w = get_word(ptr + kWindowMinW);
        w.min_h = get_word(ptr + kWindowMinH);
        const int maxw = get_word(ptr + kWindowMaxW);
        const int maxh = get_word(ptr + kWindowMaxH);
        w.max_w = maxw == 0xffff ? 0 : maxw;
        w.max_h = maxh == 0xffff ? 0 : maxh;
        w.resizable = (flags & kSizeGadget) != 0;
        w.x = x + l; w.y = y + t;
        w.w = outer_w - l - r; w.h = outer_h - t - b;
        w.title = guest_string(get_long(ptr + kWindowTitle));
        if (w.title.empty()) w.title = "Amiga window";
        if (!(flags & kBackdrop) && w.w > 0 && w.h > 0 &&
            w.x >= 0 && w.y >= 0 && w.x + w.w <= source->w &&
            w.y + w.h <= source->h)
            found.push_back(std::move(w));
        const uaecptr next = get_long(ptr + kWindowNext);
        if (!next || next == ptr) break;
        ptr = next;
    }
    return found;
}

void stop_resize(ExportedWindow& w) {
    if (w.synthetic_down) setmousebuttonstate(0, 0, 0);
    w.synthetic_down = false;
    w.resize_phase = ResizePhase::idle;
    w.pending_w = w.pending_h = 0;
}
bool resize_target(ExportedWindow& w, const GuestWindow& g, int source_w, int source_h) {
    if (w.pending_w <= 0 || w.pending_h <= 0) return false;
    const int avail_w = source_w - g.outer_x, avail_h = source_h - g.outer_y;
    if (avail_w < 1 || avail_h < 1) return false;
    const int min_w = std::clamp(g.min_w > 0 ? g.min_w : 1, 1, avail_w);
    const int min_h = std::clamp(g.min_h > 0 ? g.min_h : 1, 1, avail_h);
    const int max_w = g.max_w > 0 ? std::clamp(g.max_w, min_w, avail_w) : avail_w;
    const int max_h = g.max_h > 0 ? std::clamp(g.max_h, min_h, avail_h) : avail_h;
    const int ow = std::clamp(w.pending_w + g.left_border + g.right_border, min_w, max_w);
    const int oh = std::clamp(w.pending_h + g.top_border + g.bottom_border, min_h, max_h);
    const int inset_x = std::max(2, std::max(g.right_border, 1) / 2);
    const int inset_y = std::max(2, std::max(g.bottom_border, 1) / 2);
    w.start_x = g.outer_x + g.outer_w - inset_x;
    w.start_y = g.outer_y + g.outer_h - inset_y;
    w.target_x = w.start_x + ow - g.outer_w;
    w.target_y = w.start_y + oh - g.outer_h;
    return w.start_x >= 0 && w.start_y >= 0 && w.start_x < source_w &&
           w.start_y < source_h && w.target_x >= 0 && w.target_y >= 0 &&
           w.target_x < source_w && w.target_y < source_h;
}
void set_guest_pointer(int x, int y) {
    setmousestate(0, 0, x, 1);
    setmousestate(0, 1, y, 1);
}
void advance_resize(ExportedWindow& w, const GuestWindow& g, int sw, int sh) {
    if (!resize_enabled || !w.resizable) return;
    const uint64_t now = SDL_GetTicks();
    switch (w.resize_phase) {
    case ResizePhase::idle:
        if (!w.pending_w || !w.pending_h || now - w.pending_at < kResizeDebounce ||
            w.close_phase != ClosePhase::idle) break;
        if (!resize_target(w, g, sw, sh)) { w.pending_w = w.pending_h = 0; break; }
        w.pending_w = w.pending_h = 0;
        w.resize_start_w = g.outer_w; w.resize_start_h = g.outer_h;
        if (pause_emulation > 0 && pause_emulation <= 2)
            resumepaused_without_mouse_capture(2);
        inputdevice_acquire(FALSE);
        set_guest_pointer(w.start_x, w.start_y);
        setmousebuttonstate(0, 0, 1);
        w.synthetic_down = true;
        w.resize_phase = ResizePhase::activation_press; w.resize_phase_at = now;
        break;
    case ResizePhase::activation_press:
        if (now - w.resize_phase_at >= kResizeActivation) {
            setmousebuttonstate(0, 0, 0); w.synthetic_down = false;
            w.resize_phase = ResizePhase::activation_wait; w.resize_phase_at = now;
        }
        break;
    case ResizePhase::activation_wait:
        if (now - w.resize_phase_at >= kResizeActivationWait) {
            set_guest_pointer(w.start_x, w.start_y);
            setmousebuttonstate(0, 0, 1); w.synthetic_down = true;
            w.resize_phase = ResizePhase::press_hold; w.resize_phase_at = now;
        }
        break;
    case ResizePhase::press_hold:
        if (now - w.resize_phase_at >= kResizePressHold) {
            if (w.pending_w && w.pending_h) resize_target(w, g, sw, sh);
            w.pending_w = w.pending_h = 0;
            set_guest_pointer(w.target_x, w.target_y);
            w.resize_phase = ResizePhase::dragging; w.last_pointer_at = now;
        }
        break;
    case ResizePhase::dragging:
        if (w.pending_w && w.pending_h && now - w.last_pointer_at >= 16) {
            if (resize_target(w, g, sw, sh)) {
                set_guest_pointer(w.target_x, w.target_y);
                w.last_pointer_at = now;
            }
            w.pending_w = w.pending_h = 0;
        }
        if (now - w.pending_at >= kResizeIdle) {
            setmousebuttonstate(0, 0, 0); w.synthetic_down = false;
            w.resize_phase = ResizePhase::waiting_result; w.resize_phase_at = now;
        }
        break;
    case ResizePhase::waiting_result:
        if ((now - w.resize_phase_at >= 120 &&
             (g.outer_w != w.resize_start_w || g.outer_h != w.resize_start_h)) ||
            now - w.resize_phase_at >= kResizeResultTimeout)
            w.resize_phase = ResizePhase::idle;
        break;
    }
}
void sync_host_size(ExportedWindow& w, const GuestWindow& g) {
    if (!resize_enabled || !w.window || w.resize_phase != ResizePhase::idle ||
        w.pending_w || w.pending_h) return;
    int width = 0, height = 0;
    if (!SDL_GetWindowSize(w.window, &width, &height)) return;
    if (width == g.w && height == g.h) return;
    w.expected_host_w = g.w; w.expected_host_h = g.h;
    w.expected_until = SDL_GetTicks() + 1000;
    SDL_SetWindowSize(w.window, g.w, g.h);
}

void destroy(ExportedWindow& w) {
    stop_resize(w);
    if (w.texture) SDL_DestroyTexture(w.texture);
    if (w.renderer) SDL_DestroyRenderer(w.renderer);
    if (w.window) SDL_DestroyWindow(w.window);
    w = ExportedWindow{};
}
bool ensure(ExportedWindow& w, const GuestWindow& guest, SDL_PixelFormat format) {
    if (!w.window) {
        w.resizable = resize_enabled && guest.resizable;
        w.window = SDL_CreateWindow(guest.title.c_str(), guest.w, guest.h,
                                    SDL_WINDOW_HIGH_PIXEL_DENSITY |
                                    (w.resizable ? SDL_WINDOW_RESIZABLE : 0));
        if (!w.window) {
            write_log("AmiOS P0: SDL_CreateWindow failed: %s\n", SDL_GetError());
            return false;
        }
        w.renderer = SDL_CreateRenderer(w.window, nullptr);
        if (!w.renderer) {
            write_log("AmiOS P0: SDL_CreateRenderer failed: %s\n", SDL_GetError());
            destroy(w);
            return false;
        }
        write_log("AmiOS P0: exported guest Window* %08x as %s\n",
                  static_cast<unsigned>(guest.id), guest.title.c_str());
    }
    if (w.title != guest.title) {
        SDL_SetWindowTitle(w.window, guest.title.c_str());
        w.title = guest.title;
    }
    if (!w.texture || w.width != guest.w || w.height != guest.h ||
        w.texture_format != format) {
        if (w.texture) SDL_DestroyTexture(w.texture);
        w.texture = SDL_CreateTexture(w.renderer, format,
                                       SDL_TEXTUREACCESS_STREAMING,
                                       guest.w, guest.h);
        if (!w.texture) {
            write_log("AmiOS P0: SDL_CreateTexture failed: %s\n", SDL_GetError());
            return false;
        }
        SDL_SetTextureScaleMode(w.texture, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(w.texture, SDL_BLENDMODE_NONE);
        if (!resize_enabled) SDL_SetWindowSize(w.window, guest.w, guest.h);
        w.width = guest.w; w.height = guest.h;
        w.texture_format = format;
        w.has_frame = false;
    }
    return true;
}
SDL_WindowID event_window_id(const SDL_Event& e) {
    if (e.type >= SDL_EVENT_WINDOW_FIRST && e.type <= SDL_EVENT_WINDOW_LAST)
        return e.window.windowID;
    switch (e.type) {
    case SDL_EVENT_KEY_DOWN: case SDL_EVENT_KEY_UP: return e.key.windowID;
    case SDL_EVENT_MOUSE_BUTTON_DOWN: case SDL_EVENT_MOUSE_BUTTON_UP: return e.button.windowID;
    case SDL_EVENT_MOUSE_MOTION: return e.motion.windowID;
    case SDL_EVENT_MOUSE_WHEEL: return e.wheel.windowID;
    default: return 0;
    }
}
void show_root_if_needed() {
    if (!root_hidden || !windows.empty()) return;
    SDL_Window* root = AMonitors[monitor_id].amiga_window;
    if (root) SDL_ShowWindow(root);
    root_hidden = false;
}
} // namespace

bool amios_p0_export_enabled() {
    init();
    return enabled;
}
void amios_p0_export_present(int monid, SDL_Surface* source, bool p96_active) {
    init();
    if (!enabled || !source || !source->pixels || !p96_active || monid != monitor_id)
        return;
    const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(source->format);
    if (!fmt || fmt->bytes_per_pixel <= 0) return;
    const auto guest_windows = scan(source);
    for (auto& item : windows) item.second.seen = false;
    for (const auto& guest : guest_windows) {
        // Don't pretend to have an independent framebuffer if guest windows
        // overlap. The 68k allocator is expected to keep them separated.
        bool collision = false;
        for (const auto& other : guest_windows) {
            if (other.id == guest.id) continue;
            if (guest.x < other.x + other.w && other.x < guest.x + guest.w &&
                guest.y < other.y + other.h && other.y < guest.y + guest.h) {
                collision = true;
                break;
            }
        }
        auto& slot = windows[guest.id];
        slot.seen = true;
        slot.missed = 0;
        slot.guest_x = guest.x; slot.guest_y = guest.y;
        advance_close(slot, guest.id);
        if (collision) {
            if (SDL_GetTicks() - last_warning_ms >= 5000) {
                write_log("AmiOS P0: guest windows overlap; waiting for allocator rather than capturing contaminated pixels\n");
                last_warning_ms = SDL_GetTicks();
            }
            continue;
        }
        if (!ensure(slot, guest, source->format)) continue;
        advance_resize(slot, guest, source->w, source->h);
        sync_host_size(slot, guest);
        const auto* ptr = static_cast<const uae_u8*>(source->pixels) +
                          static_cast<size_t>(guest.y) * source->pitch +
                          static_cast<size_t>(guest.x) * fmt->bytes_per_pixel;
        if (!SDL_UpdateTexture(slot.texture, nullptr, ptr, source->pitch)) continue;
        SDL_SetRenderDrawColor(slot.renderer, 0, 0, 0, 255);
        SDL_RenderClear(slot.renderer);
        const SDL_FRect dest{0.f, 0.f, static_cast<float>(guest.w), static_cast<float>(guest.h)};
        SDL_RenderTexture(slot.renderer, slot.texture, nullptr, resize_enabled ? &dest : nullptr);
        SDL_RenderPresent(slot.renderer);
        slot.has_frame = true;
    }
    for (auto it = windows.begin(); it != windows.end();) {
        if (it->second.seen || ++it->second.missed <= kRetireAfterMisses) {
            ++it;
        } else {
            destroy(it->second);
            it = windows.erase(it);
        }
    }
    show_root_if_needed();
    if (hide_root && !root_hidden && !root_unhidden_by_user && !windows.empty()) {
        SDL_Window* root = AMonitors[monitor_id].amiga_window;
        if (root) {
            SDL_HideWindow(root);
            root_hidden = true;
        }
    }
}
bool amios_p0_export_event(const SDL_Event& e) {
    init();
    if (!enabled) return false;
    const SDL_WindowID id = event_window_id(e);
    if (!id) return false;
    ExportedWindow* target = nullptr;
    for (auto& item : windows) {
        if (item.second.window && SDL_GetWindowID(item.second.window) == id) {
            target = &item.second;
            break;
        }
    }
    if (!target) return false;
    if (e.type == SDL_EVENT_WINDOW_RESIZED) {
        if (!resize_enabled || !target->resizable) return true;
        const int width = e.window.data1, height = e.window.data2;
        if (width <= 0 || height <= 0) return true;
        if (target->expected_until >= SDL_GetTicks() &&
            target->expected_host_w == width && target->expected_host_h == height) {
            target->expected_until = 0; return true;
        }
        target->pending_w = width; target->pending_h = height;
        target->pending_at = SDL_GetTicks();
        return true;
    }
    if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        // Ask AmiOSBridge to post IECLASS_CLOSEWINDOW to the correct guest.
        // Preserve both windows if the helper/application cannot close safely.
        for (auto& item : windows) if (&item.second == target) {
            if (target->close_phase == ClosePhase::idle) {
                target->close_phase = ClosePhase::request;
                target->close_started = SDL_GetTicks();
            }
            break;
        }
        return true;
    }
    if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat &&
        e.key.scancode == SDL_SCANCODE_F12 &&
        (e.key.mod & SDL_KMOD_CTRL) && (e.key.mod & SDL_KMOD_ALT)) {
        SDL_Window* root = AMonitors[monitor_id].amiga_window;
        if (root) {
            root_unhidden_by_user = !root_unhidden_by_user;
            if (root_unhidden_by_user) {
                SDL_ShowWindow(root); SDL_RaiseWindow(root); root_hidden = false;
            } else {
                SDL_HideWindow(root); root_hidden = true;
            }
        }
        return true;
    }
    if (!input_enabled) return true;
    if (target->synthetic_down) return true;
    switch (e.type) {
    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        const float x = e.type == SDL_EVENT_MOUSE_MOTION ? e.motion.x : e.button.x;
        const float y = e.type == SDL_EVENT_MOUSE_MOTION ? e.motion.y : e.button.y;
        const SDL_MouseID which = e.type == SDL_EVENT_MOUSE_MOTION ? e.motion.which : e.button.which;
        int mouse = get_mouse_index_from_sdl_id(which);
        if (mouse < 0) mouse = 0;
        setmousestate(mouse, 0, target->guest_x + std::clamp(static_cast<int>(x), 0, target->width - 1), 1);
        setmousestate(mouse, 1, target->guest_y + std::clamp(static_cast<int>(y), 0, target->height - 1), 1);
        if (e.type != SDL_EVENT_MOUSE_MOTION) {
            const int b = e.button.button == SDL_BUTTON_LEFT ? 0 :
                          e.button.button == SDL_BUTTON_RIGHT ? 1 :
                          e.button.button == SDL_BUTTON_MIDDLE ? 2 : -1;
            if (b >= 0) setmousebuttonstate(mouse, b, e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? 1 : 0);
        }
        break;
    }
    case SDL_EVENT_KEY_DOWN: case SDL_EVENT_KEY_UP:
        inputdevice_translatekeycode(0, static_cast<int>(e.key.scancode),
                                     e.type == SDL_EVENT_KEY_DOWN ? 1 : 0, true);
        break;
    default: break;
    }
    return true;
}
