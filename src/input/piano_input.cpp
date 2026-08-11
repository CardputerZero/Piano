#include "input/piano_input.hpp"

#include <lvgl.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdlib>
#include <utility>

#if !PIANO_USE_SDL && defined(__linux__)
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace piano {
namespace {

#if !PIANO_USE_SDL && defined(__linux__)
template <std::size_t N>
bool testBit(const std::array<unsigned long, N>& bits, unsigned int bit)
{
    constexpr unsigned int kBitsPerWord = sizeof(unsigned long) * 8;
    const unsigned int index            = bit / kBitsPerWord;
    const unsigned int offset           = bit % kBitsPerWord;
    return index < bits.size() && ((bits[index] >> offset) & 1UL) != 0;
}

bool hasPianoKeys(int fd)
{
    constexpr std::size_t kKeyBitsSize = (KEY_MAX + sizeof(unsigned long) * 8) / (sizeof(unsigned long) * 8);
    std::array<unsigned long, kKeyBitsSize> key_bits{};
    if (::ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits.data()) < 0) {
        return false;
    }
    return testBit(key_bits, KEY_A) && testBit(key_bits, KEY_W) && testBit(key_bits, KEY_SPACE);
}

PianoKey pianoKeyForLinuxCode(uint16_t code)
{
    switch (code) {
        case KEY_A:
            return PianoKey::C;
        case KEY_W:
            return PianoKey::CSharp;
        case KEY_S:
            return PianoKey::D;
        case KEY_E:
            return PianoKey::DSharp;
        case KEY_D:
            return PianoKey::E;
        case KEY_F:
            return PianoKey::F;
        case KEY_T:
            return PianoKey::FSharp;
        case KEY_G:
            return PianoKey::G;
        case KEY_Y:
            return PianoKey::GSharp;
        case KEY_H:
            return PianoKey::A;
        case KEY_U:
            return PianoKey::ASharp;
        case KEY_J:
            return PianoKey::B;
        case KEY_K:
            return PianoKey::HighC;
        case KEY_LEFT:
        case KEY_Z:
            return PianoKey::OctaveDown;
        case KEY_RIGHT:
        case KEY_C:
            return PianoKey::OctaveUp;
        case KEY_SPACE:
            return PianoKey::ToggleKeymap;
        case KEY_TAB:
            return PianoKey::ToggleMode;
        case KEY_M:
            return PianoKey::ToggleTonality;
        case KEY_P:
            return PianoKey::TogglePlayalong;
        case KEY_ESC:
            return PianoKey::Escape;
        default:
            return PianoKey::Unknown;
    }
}
#endif

#if PIANO_USE_SDL
PianoKey pianoKeyForSdlScancode(SDL_Scancode scancode)
{
    switch (scancode) {
        case SDL_SCANCODE_A:
            return PianoKey::C;
        case SDL_SCANCODE_W:
            return PianoKey::CSharp;
        case SDL_SCANCODE_S:
            return PianoKey::D;
        case SDL_SCANCODE_E:
            return PianoKey::DSharp;
        case SDL_SCANCODE_D:
            return PianoKey::E;
        case SDL_SCANCODE_F:
            return PianoKey::F;
        case SDL_SCANCODE_T:
            return PianoKey::FSharp;
        case SDL_SCANCODE_G:
            return PianoKey::G;
        case SDL_SCANCODE_Y:
            return PianoKey::GSharp;
        case SDL_SCANCODE_H:
            return PianoKey::A;
        case SDL_SCANCODE_U:
            return PianoKey::ASharp;
        case SDL_SCANCODE_J:
            return PianoKey::B;
        case SDL_SCANCODE_K:
            return PianoKey::HighC;
        case SDL_SCANCODE_LEFT:
        case SDL_SCANCODE_Z:
            return PianoKey::OctaveDown;
        case SDL_SCANCODE_RIGHT:
        case SDL_SCANCODE_C:
            return PianoKey::OctaveUp;
        case SDL_SCANCODE_SPACE:
            return PianoKey::ToggleKeymap;
        case SDL_SCANCODE_TAB:
            return PianoKey::ToggleMode;
        case SDL_SCANCODE_M:
            return PianoKey::ToggleTonality;
        case SDL_SCANCODE_P:
            return PianoKey::TogglePlayalong;
        case SDL_SCANCODE_ESCAPE:
            return PianoKey::Escape;
        default:
            return PianoKey::Unknown;
    }
}
#endif

}  // namespace

PianoInput::~PianoInput()
{
    close();
}

bool PianoInput::openDefault()
{
#if PIANO_USE_SDL
    if (!_sdl_watch_registered) {
        SDL_AddEventWatch(sdlEventWatch, this);
        _sdl_watch_registered = true;
        spdlog::info("Piano input: SDL event watch enabled");
    }
    return true;
#elif defined(__linux__)
    constexpr const char* kEnvironmentVariables[] = {
        "PIANO_KEYBOARD_DEVICE",
        "APPLAUNCH_LINUX_KEYBOARD_DEVICE",
        "LV_LINUX_KEYBOARD_DEVICE",
    };
    for (const char* variable : kEnvironmentVariables) {
        const char* path = std::getenv(variable);
        if (path && path[0] != '\0') {
            return openDevice(path, false);
        }
    }
    if (openDevice("/dev/input/by-path/platform-3f804000.i2c-event", false)) {
        return true;
    }
    for (int index = 0; index < 32; ++index) {
        if (openDevice("/dev/input/event" + std::to_string(index), true)) {
            return true;
        }
    }
    spdlog::warn("Piano input: no compatible input device found");
#endif
    return false;
}

bool PianoInput::openDevice(const std::string& path, bool require_piano_keys)
{
#if !PIANO_USE_SDL && defined(__linux__)
    const int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }
    if (require_piano_keys && !hasPianoKeys(fd)) {
        ::close(fd);
        return false;
    }
    _event_devices.push_back({fd, false});
    spdlog::info("Piano input: opened {} (shared, no EVIOCGRAB)", path);
    return true;
#else
    (void)path;
    (void)require_piano_keys;
    return false;
#endif
}

void PianoInput::close()
{
#if PIANO_USE_SDL
    if (_sdl_watch_registered) {
        SDL_DelEventWatch(sdlEventWatch, this);
        _sdl_watch_registered = false;
    }
    {
        std::lock_guard<std::mutex> lock(_sdl_event_mutex);
        _sdl_events.clear();
    }
#elif defined(__linux__)
    for (const EventDevice& device : _event_devices) {
        if (device.fd >= 0) {
            ::close(device.fd);
        }
    }
#endif
    _event_devices.clear();
    emitAllReleased();
}

void PianoInput::poll()
{
#if PIANO_USE_SDL
    std::vector<PianoInputEvent> events;
    {
        std::lock_guard<std::mutex> lock(_sdl_event_mutex);
        events.swap(_sdl_events);
    }
    for (const PianoInputEvent& event : events) {
        if (event.type == PianoInputEventType::AllReleased) {
            emitAllReleased();
        } else {
            emit(event.key, event.pressed, event.repeated);
        }
    }
#elif defined(__linux__)
    for (EventDevice& device : _event_devices) {
        if (device.fd < 0) {
            continue;
        }
        while (true) {
            input_event event{};
            const ssize_t bytes_read = ::read(device.fd, &event, sizeof(event));
            if (bytes_read == sizeof(event)) {
                if (event.type == EV_SYN && event.code == SYN_DROPPED) {
                    device.dropped = true;
                    emitAllReleased();
                    spdlog::warn("Piano input: event stream dropped; released all notes");
                    continue;
                }
                if (device.dropped) {
                    if (event.type == EV_SYN && event.code == SYN_REPORT) {
                        device.dropped = false;
                    }
                    continue;
                }
                if (event.type == EV_KEY && (event.value == 0 || event.value == 1 || event.value == 2)) {
                    const PianoKey key = pianoKeyForLinuxCode(event.code);
                    if (key != PianoKey::Unknown) {
                        emit(key, event.value != 0, event.value == 2);
                    }
                }
                continue;
            }
            if (bytes_read < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
                break;
            }
            if (bytes_read < 0) {
                spdlog::warn("Piano input: read failed: {}", std::strerror(errno));
                emitAllReleased();
            } else if (bytes_read == 0) {
                spdlog::warn("Piano input: device disconnected");
                emitAllReleased();
                ::close(device.fd);
                device.fd = -1;
            }
            break;
        }
    }
    _event_devices.erase(std::remove_if(_event_devices.begin(), _event_devices.end(),
                                        [](const EventDevice& device) { return device.fd < 0; }),
                         _event_devices.end());
#endif
}

void PianoInput::setEventCallback(EventCallback callback)
{
    _callback = std::move(callback);
}

void PianoInput::emit(PianoKey key, bool pressed, bool repeated)
{
    const auto index = static_cast<std::size_t>(key);
    if (key == PianoKey::Unknown || index >= _pressed_keys.size() || _pressed_keys[index] == pressed) {
        return;
    }
    _pressed_keys[index] = pressed;
    if (_callback) {
        _callback({PianoInputEventType::Key, key, pressed, repeated, lv_tick_get()});
    }
}

void PianoInput::emitAllReleased()
{
    const bool had_pressed_keys =
        std::any_of(_pressed_keys.begin(), _pressed_keys.end(), [](bool pressed) { return pressed; });
    _pressed_keys.fill(false);
    if (had_pressed_keys && _callback) {
        _callback({PianoInputEventType::AllReleased, PianoKey::Unknown, false, false, lv_tick_get()});
    }
}

#if PIANO_USE_SDL
void PianoInput::queueSdlEvent(const PianoInputEvent& event)
{
    std::lock_guard<std::mutex> lock(_sdl_event_mutex);
    _sdl_events.push_back(event);
}

int PianoInput::sdlEventWatch(void* context, SDL_Event* event)
{
    auto* input = static_cast<PianoInput*>(context);
    if (!input || !event) {
        return 1;
    }

    if (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
        input->queueSdlEvent(
            {PianoInputEventType::AllReleased, PianoKey::Unknown, false, false, event->window.timestamp});
        return 1;
    }
    if (event->type != SDL_KEYDOWN && event->type != SDL_KEYUP) {
        return 1;
    }

    const PianoKey key = pianoKeyForSdlScancode(event->key.keysym.scancode);
    if (key != PianoKey::Unknown) {
        input->queueSdlEvent({PianoInputEventType::Key, key, event->type == SDL_KEYDOWN,
                              event->type == SDL_KEYDOWN && event->key.repeat != 0, event->key.timestamp});
    }
    return 1;
}
#endif

}  // namespace piano
