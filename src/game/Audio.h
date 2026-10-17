/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/sim/Mission.h"

namespace fossynd::game
{

// Sound-effect cues (ADR-0040). SFX are in scope for v1.0.0; music is not, so
// there is no streamed/looping path here. Every cue is *generated* in code, so
// the project ships no audio file and cannot copy the original's sounds (golden
// rule 8). Add a cue to this enum and the generator, never as an asset.
enum class Cue
{
    Shoot = 0,
    Hit,
    Explosion,
    Victory,
    Defeat,
    Count,
};

constexpr std::size_t kCueCount = static_cast<std::size_t>(Cue::Count);

// The cue a mission outcome should play. `Running` has no outcome cue (the
// caller passes only a decided state), but mapping it is defined so the helper
// is total. Pure and device-free, so the headless guard can check it.
Cue cueForState(core::MissionState state);

// A frame's mission events, as the audio trigger sees them (issue #423). The
// core computes all of it (the kill counts and the state); the view only
// snapshots it. `anyShot` is the one piece the view derives, from an agent's
// gunfire timer rising since the previous frame. Plain data - no device, no
// engine type - so the mapping below is testable headless.
struct AudioEvents
{
    int playerKills = 0;
    int enemyKills = 0;
    int civilianKills = 0;
    core::MissionState state = core::MissionState::Running;
    bool anyShot = false;
};

// The cues one frame should play, given the previous frame's events and this
// frame's. Pure: a *change* fires a cue (a new kill, a decided outcome), a
// steady state fires nothing, and a shot fires on `anyShot`. Kept free of the
// device so the headless guard exercises the real decision, not a copy of it.
std::vector<Cue> cuesForFrame(const AudioEvents& previous, const AudioEvents& current);

// The engine-layer sound-effect player (ADR-0040). It owns one SDL audio device
// and a small fixed set of procedurally synthesised cues, mixed into a streaming
// buffer that `update()` feeds to the device. It is deliberately **never fatal**:
// when no device can be opened (a headless run, a machine with no sound card, a
// dummy driver) it logs once and degrades to a silent no-op, so `enabled()` is
// false and every later call is harmless.
//
// This includes <SDL.h>, so it lives in the engine layer and never in `src/core`
// (golden rule 1). Audio follows the view: `Game` triggers it from state the core
// already computes; the core itself never plays a sound.
class Audio
{
public:
    Audio() = default;
    ~Audio();

    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    // Opens the output device and bakes the cue table. Returns true when audio
    // is live, false when it degraded to silence — the caller keeps running
    // either way (a missing device is not an error, AGENTS.md 1b).
    bool init();

    // Closes the device. Safe to call twice and safe when init() failed.
    void shutdown();

    // True when a device is open and `play` produces sound.
    bool enabled() const { return device_ != 0; }

    // Starts a cue. A no-op when audio is disabled, so callers need no guard.
    void play(Cue cue);

    // Mixes the active cues into the device's stream. Call once per frame with
    // the number of frames drawn; a no-op when audio is disabled.
    void update();

    // Headless self-test (ctest `game_audio`): the cue table is complete and
    // non-silent, synthesis is deterministic, mixing sums cues, and a disabled
    // Audio is a safe no-op. Needs no device, so CI can run it.
    static bool selfTest();

private:
    // One baked cue: 16-bit signed mono samples at kSampleRate.
    struct CueSamples
    {
        std::vector<std::int16_t> samples;
        std::size_t length() const { return samples.size(); }
    };

    // An in-flight cue: which baked cue and how far through it we are.
    struct Voice
    {
        std::size_t cue = 0;
        std::size_t position = 0;
    };

    static std::vector<std::int16_t> synthesise(Cue cue);

    // Sums the active voices into `out` (kChunkFrames samples) and advances
    // them, dropping finished ones. Device-free, so selfTest() can drive it.
    void mixInto(std::vector<std::int16_t>& out);
    void advanceVoices(std::size_t frames);

    std::uint32_t device_ = 0;
    std::vector<CueSamples> cues_;
    std::vector<Voice> voices_;
};

} // namespace fossynd::game
