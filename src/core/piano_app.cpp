#include "core/piano_app.hpp"

#include "assets/font_assets.hpp"

#include <lvgl.h>
#include <spdlog/spdlog.h>

namespace piano {

PianoApp::PianoApp() : _model(_synth), _view_model(_model), _view(_view_model)
{
}

PianoApp::~PianoApp()
{
    stop();
}

bool PianoApp::start(lv_obj_t* parent)
{
    if (_started || !parent) {
        return _started;
    }

    initFontAssets();
    if (!_synth.start()) {
        spdlog::warn("Piano: synthesizer is unavailable; continuing with silent keyboard UI");
    }
    _view_model.onEnter();
    _view.onEnter(parent);
    _input.setEventCallback([this](const PianoInputEvent& event) { onInput(event); });
    if (!_input.openDefault()) {
        spdlog::warn("Piano: keyboard input is unavailable");
    }
    _quit_requested = false;
    _escape_down    = false;
    _started        = true;
    spdlog::info("Piano: started");
    return true;
}

void PianoApp::stop()
{
    if (!_started) {
        return;
    }
    _input.close();
    _model.releaseAll(true);
    _view.onExit();
    _view_model.onExit();
    _synth.stop();
    _started = false;
}

void PianoApp::tick(uint32_t now_ms)
{
    if (!_started) {
        return;
    }

    _input.poll();
    _model.tick(now_ms);
    if (_escape_down && now_ms - _escape_pressed_at >= kExitHoldMs) {
        _quit_requested = true;
        _escape_down    = false;
        _model.releaseAll(true);
    }
    _view.tick(now_ms);
}

bool PianoApp::quitRequested() const
{
    return _quit_requested;
}

void PianoApp::onInput(const PianoInputEvent& event)
{
    if (event.type == PianoInputEventType::AllReleased) {
        _escape_down = false;
        _view_model.onInput(event);
        return;
    }
    if (event.key == PianoKey::Escape) {
        if (event.pressed && !event.repeated && !_escape_down) {
            _escape_down       = true;
            _escape_pressed_at = event.timestamp_ms;
        } else if (!event.pressed) {
            _escape_down = false;
        }
        return;
    }
    if (event.key == PianoKey::Help) {
        if (event.pressed && !event.repeated) {
            const bool was_visible = _view.helpVisible();
            _view.toggleHelp();
            if (!was_visible) {
                _model.releaseAll(true);
            }
        }
        return;
    }
    if (_view.helpVisible()) {
        return;
    }
    _view_model.onInput(event);
}

}  // namespace piano
