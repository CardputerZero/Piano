#pragma once

#include <cstdint>
#include <memory>

namespace piano {

using VoiceHandle = std::uint32_t;

class SynthOutput {
public:
    virtual ~SynthOutput() = default;

    virtual void noteOn(VoiceHandle handle, int midi_note, float velocity) = 0;
    virtual void noteOff(VoiceHandle handle)                               = 0;
    virtual void allNotesOff(bool immediate)                               = 0;
};

class SynthEngine final : public SynthOutput {
public:
    SynthEngine();
    ~SynthEngine() override;

    SynthEngine(const SynthEngine&)            = delete;
    SynthEngine& operator=(const SynthEngine&) = delete;

    bool start();
    void stop();
    bool running() const;

    void noteOn(VoiceHandle handle, int midi_note, float velocity) override;
    void noteOff(VoiceHandle handle) override;
    void allNotesOff(bool immediate) override;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

}  // namespace piano
