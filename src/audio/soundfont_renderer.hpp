#pragma once

#include "audio/synth_engine.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

struct tsf;

namespace piano {

class SoundFontRenderer {
public:
    static constexpr std::size_t kTriggerSlotCount = 24;
    static constexpr int kMaximumVoiceCount        = 64;

    SoundFontRenderer() = default;
    ~SoundFontRenderer();

    SoundFontRenderer(const SoundFontRenderer&)            = delete;
    SoundFontRenderer& operator=(const SoundFontRenderer&) = delete;

    bool load(const std::string& path, int sample_rate);
    bool ready() const;
    const std::string& presetName() const;

    void noteOn(VoiceHandle handle, int midi_note, float velocity);
    void noteOff(VoiceHandle handle);
    void allNotesOff(bool immediate);
    void render(float* interleaved, std::size_t frame_count, std::size_t channels);
    std::size_t activeVoiceCount() const;

private:
    struct TriggerSlot {
        VoiceHandle handle = 0;
        int midi_note      = 0;
        std::uint64_t age  = 0;
        bool active        = false;
    };

    ::tsf* _font = nullptr;
    std::array<TriggerSlot, kTriggerSlotCount> _triggers{};
    std::uint64_t _next_age = 1;
    std::string _preset_name;

    TriggerSlot* findTrigger(VoiceHandle handle);
    TriggerSlot& selectTrigger();
    std::size_t channelFor(const TriggerSlot& trigger) const;
    void close();
};

}  // namespace piano
