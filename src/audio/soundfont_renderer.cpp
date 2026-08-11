#include "audio/soundfont_renderer.hpp"

#define TSF_IMPLEMENTATION
#include <tsf.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace piano {
namespace {

constexpr int kPianoPresetIndex = 0;
constexpr float kMasterGainDb   = -11.0f;

}  // namespace

SoundFontRenderer::~SoundFontRenderer()
{
    close();
}

bool SoundFontRenderer::load(const std::string& path, int sample_rate)
{
    if (path.empty() || sample_rate <= 0) {
        return false;
    }

    ::tsf* candidate = tsf_load_filename(path.c_str());
    if (!candidate || tsf_get_presetcount(candidate) <= kPianoPresetIndex) {
        tsf_close(candidate);
        return false;
    }

    tsf_set_output(candidate, TSF_STEREO_INTERLEAVED, sample_rate, kMasterGainDb);
    if (!tsf_set_max_voices(candidate, kMaximumVoiceCount) ||
        !tsf_channel_set_presetindex(candidate, static_cast<int>(kTriggerSlotCount - 1), kPianoPresetIndex)) {
        tsf_close(candidate);
        return false;
    }
    for (std::size_t channel = 0; channel < kTriggerSlotCount; ++channel) {
        if (!tsf_channel_set_presetindex(candidate, static_cast<int>(channel), kPianoPresetIndex)) {
            tsf_close(candidate);
            return false;
        }
    }

    const char* preset_name = tsf_get_presetname(candidate, kPianoPresetIndex);
    close();
    _font        = candidate;
    _preset_name = preset_name ? preset_name : "Piano";
    return true;
}

bool SoundFontRenderer::ready() const
{
    return _font != nullptr;
}

const std::string& SoundFontRenderer::presetName() const
{
    return _preset_name;
}

void SoundFontRenderer::noteOn(VoiceHandle handle, int midi_note, float velocity)
{
    if (!_font || handle == 0) {
        return;
    }
    if (velocity <= 0.0f) {
        noteOff(handle);
        return;
    }

    TriggerSlot* trigger = findTrigger(handle);
    if (!trigger) {
        trigger = &selectTrigger();
    }
    const int channel = static_cast<int>(channelFor(*trigger));
    if (trigger->active) {
        tsf_channel_sounds_off_all(_font, channel);
    }

    trigger->handle    = handle;
    trigger->midi_note = std::clamp(midi_note, 0, 127);
    trigger->age       = _next_age++;
    trigger->active    = true;
    if (!tsf_channel_note_on(_font, channel, trigger->midi_note, std::clamp(velocity, 0.0f, 1.0f))) {
        *trigger = {};
    }
}

void SoundFontRenderer::noteOff(VoiceHandle handle)
{
    if (!_font || handle == 0) {
        return;
    }

    TriggerSlot* trigger = findTrigger(handle);
    if (!trigger) {
        return;
    }
    tsf_channel_note_off(_font, static_cast<int>(channelFor(*trigger)), trigger->midi_note);
    trigger->active = false;
    trigger->handle = 0;
}

void SoundFontRenderer::allNotesOff(bool immediate)
{
    if (!_font) {
        return;
    }

    for (std::size_t channel = 0; channel < kTriggerSlotCount; ++channel) {
        if (immediate) {
            tsf_channel_sounds_off_all(_font, static_cast<int>(channel));
        } else {
            tsf_channel_note_off_all(_font, static_cast<int>(channel));
        }
    }
    _triggers.fill({});
}

void SoundFontRenderer::render(float* interleaved, std::size_t frame_count, std::size_t channels)
{
    if (!interleaved || channels == 0) {
        return;
    }
    if (!_font || channels != 2) {
        std::fill(interleaved, interleaved + frame_count * channels, 0.0f);
        return;
    }

    float* output              = interleaved;
    std::size_t frames_pending = frame_count;
    while (frames_pending > 0) {
        const int frames =
            static_cast<int>(std::min(frames_pending, static_cast<std::size_t>(std::numeric_limits<int>::max())));
        tsf_render_float(_font, output, frames, 0);
        output += static_cast<std::size_t>(frames) * channels;
        frames_pending -= static_cast<std::size_t>(frames);
    }

    for (float* sample = interleaved; sample != interleaved + frame_count * channels; ++sample) {
        *sample = std::isfinite(*sample) ? std::clamp(*sample, -1.0f, 1.0f) : 0.0f;
    }
}

std::size_t SoundFontRenderer::activeVoiceCount() const
{
    return _font ? static_cast<std::size_t>(std::max(0, tsf_active_voice_count(_font))) : 0;
}

SoundFontRenderer::TriggerSlot* SoundFontRenderer::findTrigger(VoiceHandle handle)
{
    const auto trigger = std::find_if(_triggers.begin(), _triggers.end(), [handle](const TriggerSlot& candidate) {
        return candidate.active && candidate.handle == handle;
    });
    return trigger == _triggers.end() ? nullptr : &*trigger;
}

SoundFontRenderer::TriggerSlot& SoundFontRenderer::selectTrigger()
{
    const auto free_trigger =
        std::find_if(_triggers.begin(), _triggers.end(), [](const TriggerSlot& trigger) { return !trigger.active; });
    if (free_trigger != _triggers.end()) {
        return *free_trigger;
    }

    auto oldest =
        std::min_element(_triggers.begin(), _triggers.end(),
                         [](const TriggerSlot& left, const TriggerSlot& right) { return left.age < right.age; });
    tsf_channel_sounds_off_all(_font, static_cast<int>(channelFor(*oldest)));
    return *oldest;
}

std::size_t SoundFontRenderer::channelFor(const TriggerSlot& trigger) const
{
    return static_cast<std::size_t>(&trigger - _triggers.data());
}

void SoundFontRenderer::close()
{
    tsf_close(_font);
    _font = nullptr;
    _triggers.fill({});
    _next_age = 1;
    _preset_name.clear();
}

}  // namespace piano
