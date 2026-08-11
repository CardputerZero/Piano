#pragma once

#include "models/piano_model.hpp"

namespace piano {

class PianoViewModel {
public:
    explicit PianoViewModel(PianoModel& model);

    void onEnter();
    void onExit();
    void onInput(const PianoInputEvent& event);
    smooth_ui_toolkit::SingleObservable<PianoState>& state();

private:
    PianoModel& _model;
};

}  // namespace piano
