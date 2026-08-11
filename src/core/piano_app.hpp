#pragma once

#include "audio/synth_engine.hpp"
#include "input/piano_input.hpp"
#include "models/piano_model.hpp"
#include "view_models/piano_view_model.hpp"
#include "views/piano_view.hpp"

#include <cstdint>

namespace piano {

class PianoApp {
public:
    PianoApp();
    ~PianoApp();

    bool start(lv_obj_t* parent);
    void stop();
    void tick(uint32_t now_ms);
    bool quitRequested() const;

private:
    static constexpr uint32_t kExitHoldMs = 900;

    SynthEngine _synth;
    PianoModel _model;
    PianoViewModel _view_model;
    PianoView _view;
    PianoInput _input;
    bool _started               = false;
    bool _quit_requested        = false;
    bool _escape_down           = false;
    uint32_t _escape_pressed_at = 0;

    void onInput(const PianoInputEvent& event);
};

}  // namespace piano
