#include "view_models/piano_view_model.hpp"

#include <spdlog/spdlog.h>

namespace piano {

PianoViewModel::PianoViewModel(PianoModel& model) : _model(model)
{
}

void PianoViewModel::onEnter()
{
    _model.reset();
    spdlog::info("Piano: keyboard entered");
}

void PianoViewModel::onExit()
{
}

void PianoViewModel::onInput(const PianoInputEvent& event)
{
    _model.handleInput(event);
}

smooth_ui_toolkit::SingleObservable<PianoState>& PianoViewModel::state()
{
    return _model.state();
}

}  // namespace piano
