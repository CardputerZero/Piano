#pragma once

#include "core/piano_types.hpp"

#include <cstdint>
#include <array>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#if PIANO_USE_SDL
#include <SDL.h>
#endif

namespace piano {

class PianoInput {
public:
    using EventCallback = std::function<void(const PianoInputEvent&)>;

    PianoInput() = default;
    ~PianoInput();

    PianoInput(const PianoInput&)            = delete;
    PianoInput& operator=(const PianoInput&) = delete;

    bool openDefault();
    void close();
    void poll();
    void setEventCallback(EventCallback callback);

private:
    struct EventDevice {
        int fd       = -1;
        bool dropped = false;
    };

    EventCallback _callback;
    std::vector<EventDevice> _event_devices;
    std::array<bool, static_cast<std::size_t>(PianoKey::Unknown)> _pressed_keys{};

    bool openDevice(const std::string& path, bool require_piano_keys);
    void emit(PianoKey key, bool pressed, bool repeated);
    void emitAllReleased();

#if PIANO_USE_SDL
    bool _sdl_watch_registered = false;
    std::mutex _sdl_event_mutex;
    std::vector<PianoInputEvent> _sdl_events;
    void queueSdlEvent(const PianoInputEvent& event);
    static int sdlEventWatch(void* context, SDL_Event* event);
#endif
};

}  // namespace piano
