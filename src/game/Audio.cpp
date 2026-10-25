/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "game/Audio.h"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace fossynd::game
{

namespace
{

// 22.05 kHz mono 16-bit is plenty for short blips and keeps the buffers small
// for the low-spec target (AGENTS.md 1b). No resampling, no mixing library.
constexpr int kSampleRate = 22050;
constexpr int kChannels = 1;
constexpr std::size_t kChunkFrames = 1024;

// The SDL audio format we ask for. `AUDIO_S16SYS` is signed 16-bit in the
// machine's native byte order, so the samples we synthesise can be queued
// directly with no byte swap.
constexpr SDL_AudioFormat kFormat = AUDIO_S16SYS;

// Cue lengths in seconds. Kept short: these are UI/feedback blips, not music.
constexpr float kShootSeconds = 0.06f;
constexpr float kHitSeconds = 0.05f;
constexpr float kExplosionSeconds = 0.30f;
constexpr float kOutcomeSeconds = 0.45f;
// UI blips are shorter still, so a menu move does not smear into the next key.
constexpr float kSelectSeconds = 0.05f;
constexpr float kNavigateSeconds = 0.035f;
constexpr float kCancelSeconds = 0.07f;

float secondsFor(Cue cue)
{
    switch (cue)
    {
    case Cue::Shoot:
        return kShootSeconds;
    case Cue::Hit:
        return kHitSeconds;
    case Cue::Explosion:
        return kExplosionSeconds;
    case Cue::Victory:
    case Cue::Defeat:
        return kOutcomeSeconds;
    case Cue::Select:
        return kSelectSeconds;
    case Cue::Navigate:
        return kNavigateSeconds;
    case Cue::Cancel:
        return kCancelSeconds;
    case Cue::Count:
        break;
    }
    return kHitSeconds;
}

// A tiny, dependency-free deterministic noise source. `nextNoise` advances the
// state, so synthesis is reproducible and the self-test can compare two bakes.
std::uint32_t nextNoise(std::uint32_t& state)
{
    state = state * 1664525u + 1013904223u;
    return state;
}

// A linear attack/decay envelope in [0, 1] over `t` in [0, 1).
float envelope(float t, float attack)
{
    if (t < attack && attack > 0.0f)
    {
        return t / attack;
    }
    const float decay = 1.0f - t;
    return std::max(0.0f, decay);
}

std::int16_t toSample(float value)
{
    const float clamped = std::max(-1.0f, std::min(1.0f, value));
    return static_cast<std::int16_t>(std::lround(clamped * 32000.0f));
}

} // namespace

Cue cueForState(core::MissionState state)
{
    switch (state)
    {
    case core::MissionState::Won:
        return Cue::Victory;
    case core::MissionState::Lost:
        return Cue::Defeat;
    case core::MissionState::Running:
        break;
    }
    // No outcome yet: a benign, quiet cue. Callers only play on a decided state.
    return Cue::Hit;
}

Cue cueForUiAction(UiAction action)
{
    switch (action)
    {
    case UiAction::Select:
        return Cue::Select;
    case UiAction::Navigate:
        return Cue::Navigate;
    case UiAction::Cancel:
        return Cue::Cancel;
    case UiAction::Count:
        break;
    }
    // Total: an unknown action is a quiet navigation tick, never a combat cue.
    return Cue::Navigate;
}

std::vector<UiAction> uiActionsForChange(const UiAudioState& previous, const UiAudioState& current)
{
    std::vector<UiAction> actions;

    if (previous.selection != current.selection)
    {
        actions.push_back(UiAction::Select);
    }

    // Opening an overlay (the map or the R&D shop) or pausing is a navigation
    // tick; closing one or resuming is a cancel. A change in the other direction
    // on the same frame would fire both, which is why each side is tested
    // independently against the previous state.
    if ((current.campaignVisible && !previous.campaignVisible) ||
        (current.shopOpen && !previous.shopOpen) || (current.paused && !previous.paused))
    {
        actions.push_back(UiAction::Navigate);
    }
    if ((previous.campaignVisible && !current.campaignVisible) ||
        (previous.shopOpen && !current.shopOpen) || (previous.paused && !current.paused))
    {
        actions.push_back(UiAction::Cancel);
    }

    return actions;
}

std::vector<Cue> cuesForFrame(const AudioEvents& previous, const AudioEvents& current)
{
    std::vector<Cue> cues;

    // A shot this frame. One cue per frame, so a firefight does not stack.
    if (current.anyShot)
    {
        cues.push_back(Cue::Shoot);
    }

    // A new kill is a hit; a civilian killed by stray fire is an explosion (the
    // louder, more consequential event).
    if (current.playerKills > previous.playerKills ||
        current.enemyKills > previous.enemyKills)
    {
        cues.push_back(Cue::Hit);
    }
    if (current.civilianKills > previous.civilianKills)
    {
        cues.push_back(Cue::Explosion);
    }

    // The outcome fires exactly once, on the frame the mission stops running.
    if (current.state != core::MissionState::Running &&
        previous.state == core::MissionState::Running)
    {
        cues.push_back(cueForState(current.state));
    }

    return cues;
}

AudioEvents snapshotAudioEvents(const core::Mission& mission, std::vector<float>& previousGunfire)
{
    // The one place the view turns core state into audio events (issue #424), so
    // the headless guard can run the real derivation on a scratch mission instead
    // of duplicating it. A pure read of the mission (golden rule 4).
    AudioEvents current;
    current.playerKills = mission.playerKills();
    current.enemyKills = mission.enemyKills();
    current.civilianKills = mission.civilianKills();
    current.state = mission.state();

    // `Agent::beginFireCooldown` raises an agent's gunfire timer the instant it
    // fires, and the timer only ever counts down, so a timer that rose since last
    // frame means the agent fired this frame.
    const auto& agents = mission.agents();
    if (previousGunfire.size() != agents.size())
    {
        previousGunfire.assign(agents.size(), 0.0f);
    }
    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        const float timer = agents[i].gunfireTimer();
        if (timer > previousGunfire[i])
        {
            current.anyShot = true;
        }
        previousGunfire[i] = timer;
    }
    return current;
}

Audio::~Audio()
{
    shutdown();
}

bool Audio::init()
{
    if (device_ != 0)
    {
        return true;
    }

    // Bake the cues first so a later `play` cannot allocate on the audio thread
    // path. This is device-free and cannot fail.
    cues_.clear();
    cues_.reserve(kCueCount);
    for (std::size_t i = 0; i < kCueCount; ++i)
    {
        CueSamples baked;
        baked.samples = synthesise(static_cast<Cue>(i));
        cues_.push_back(std::move(baked));
    }
    voices_.clear();

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
    {
        std::fprintf(stderr, "audio: disabled (%s)\n", SDL_GetError());
        return false;
    }

    SDL_AudioSpec want{};
    want.freq = kSampleRate;
    want.format = kFormat;
    want.channels = kChannels;
    want.samples = static_cast<Uint16>(kChunkFrames);

    SDL_AudioSpec have{};
    // `allowed_changes = 0`: we only support the exact format we baked, so a
    // device that would need a conversion is treated as "no audio" rather than
    // playing the wrong pitch. Silence is an acceptable outcome (ADR-0040).
    device_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (device_ == 0)
    {
        std::fprintf(stderr, "audio: disabled (%s)\n", SDL_GetError());
        return false;
    }

    SDL_PauseAudioDevice(device_, 0);
    return true;
}

void Audio::shutdown()
{
    if (device_ != 0)
    {
        SDL_CloseAudioDevice(device_);
        device_ = 0;
    }
    voices_.clear();
    cues_.clear();
}

void Audio::play(Cue cue)
{
    if (device_ == 0)
    {
        return;
    }
    const std::size_t index = static_cast<std::size_t>(cue);
    if (index >= cues_.size() || cues_[index].samples.empty())
    {
        return;
    }
    // Bound the voice list so a burst of shots cannot grow it without limit; the
    // oldest voice is dropped, which is inaudible at these cue lengths.
    constexpr std::size_t kMaxVoices = 16;
    if (voices_.size() >= kMaxVoices)
    {
        voices_.erase(voices_.begin());
    }
    voices_.push_back(Voice{index, 0});
}

void Audio::update()
{
    if (device_ == 0)
    {
        return;
    }

    // Only queue while the device wants more, so the buffer cannot grow without
    // bound if `update` is called faster than playback drains it.
    const Uint32 queuedBytes = SDL_GetQueuedAudioSize(device_);
    const Uint32 chunkBytes =
        static_cast<Uint32>(kChunkFrames * sizeof(std::int16_t) * kChannels);
    constexpr Uint32 kMaxQueuedBytes = chunkBytes * 4;
    if (queuedBytes >= kMaxQueuedBytes)
    {
        return;
    }

    std::vector<std::int16_t> out(kChunkFrames, 0);
    mixInto(out);
    SDL_QueueAudio(device_, out.data(),
                   static_cast<Uint32>(out.size() * sizeof(std::int16_t)));
}

void Audio::mixInto(std::vector<std::int16_t>& out)
{
    const std::size_t frames = out.size();
    if (voices_.empty() || cues_.empty())
    {
        advanceVoices(frames);
        return;
    }

    for (std::size_t f = 0; f < frames; ++f)
    {
        float mixed = static_cast<float>(out[f]) / 32000.0f;
        for (const Voice& voice : voices_)
        {
            const CueSamples& cue = cues_[voice.cue];
            const std::size_t index = voice.position + f;
            if (index < cue.samples.size())
            {
                mixed += static_cast<float>(cue.samples[index]) / 32000.0f;
            }
        }
        out[f] = toSample(mixed);
    }
    advanceVoices(frames);
}

void Audio::advanceVoices(std::size_t frames)
{
    for (Voice& voice : voices_)
    {
        voice.position += frames;
    }
    voices_.erase(
        std::remove_if(voices_.begin(), voices_.end(),
                       [this](const Voice& voice) {
                           return voice.cue >= cues_.size() ||
                                  voice.position >= cues_[voice.cue].samples.size();
                       }),
        voices_.end());
}

std::vector<std::int16_t> Audio::synthesise(Cue cue)
{
    const int total = static_cast<int>(secondsFor(cue) * static_cast<float>(kSampleRate));
    std::vector<std::int16_t> samples(static_cast<std::size_t>(total), 0);
    std::uint32_t noise = 0x1234567u + static_cast<std::uint32_t>(cue);

    for (int i = 0; i < total; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(total);
        float value = 0.0f;

        switch (cue)
        {
        case Cue::Shoot:
        {
            // A short downward chirp: a square-ish blip with a fast decay.
            const float freq = 880.0f - 500.0f * t;
            const float phase = 2.0f * 3.14159265f * freq *
                                static_cast<float>(i) / static_cast<float>(kSampleRate);
            value = (std::sin(phase) >= 0.0f ? 1.0f : -1.0f) *
                    envelope(t, 0.02f) * 0.5f;
            break;
        }
        case Cue::Hit:
        {
            // A clipped low tone: a duller impact.
            const float freq = 220.0f;
            const float phase = 2.0f * 3.14159265f * freq *
                                static_cast<float>(i) / static_cast<float>(kSampleRate);
            value = std::sin(phase) * envelope(t, 0.05f) * 0.6f;
            break;
        }
        case Cue::Explosion:
        {
            // Noise with a long decay, band-limited by averaging two draws.
            const float n0 = static_cast<float>(nextNoise(noise) >> 8) / 8388608.0f - 1.0f;
            const float n1 = static_cast<float>(nextNoise(noise) >> 8) / 8388608.0f - 1.0f;
            value = (0.5f * (n0 + n1)) * envelope(t, 0.01f) * 0.7f;
            break;
        }
        case Cue::Victory:
        {
            // A rising three-note arpeggio: C5 - E5 - G5.
            const int note = std::min(2, static_cast<int>(t * 3.0f));
            const float freqs[3] = {523.25f, 659.25f, 783.99f};
            const float phase = 2.0f * 3.14159265f * freqs[note] *
                                static_cast<float>(i) / static_cast<float>(kSampleRate);
            const float local = t * 3.0f - static_cast<float>(note);
            value = std::sin(phase) * envelope(local, 0.05f) * 0.5f;
            break;
        }
        case Cue::Defeat:
        {
            // A falling two-note motif: G4 - C4.
            const int note = std::min(1, static_cast<int>(t * 2.0f));
            const float freqs[2] = {392.0f, 261.63f};
            const float phase = 2.0f * 3.14159265f * freqs[note] *
                                static_cast<float>(i) / static_cast<float>(kSampleRate);
            const float local = t * 2.0f - static_cast<float>(note);
            value = std::sin(phase) * envelope(local, 0.05f) * 0.5f;
            break;
        }
        case Cue::Select:
        {
            // A single clean rising blip: a bright confirmation "click".
            const float freq = 660.0f + 220.0f * t;
            const float phase = 2.0f * 3.14159265f * freq *
                                static_cast<float>(i) / static_cast<float>(kSampleRate);
            value = std::sin(phase) * envelope(t, 0.1f) * 0.4f;
            break;
        }
        case Cue::Navigate:
        {
            // A short, quiet high tick: a menu move, quieter than a selection.
            const float freq = 990.0f;
            const float phase = 2.0f * 3.14159265f * freq *
                                static_cast<float>(i) / static_cast<float>(kSampleRate);
            value = std::sin(phase) * envelope(t, 0.15f) * 0.28f;
            break;
        }
        case Cue::Cancel:
        {
            // A falling two-tone blip: a lower "back/cancel" cue, distinct from
            // the rising Select and the high Navigate.
            const int note = std::min(1, static_cast<int>(t * 2.0f));
            const float freqs[2] = {440.0f, 330.0f};
            const float phase = 2.0f * 3.14159265f * freqs[note] *
                                static_cast<float>(i) / static_cast<float>(kSampleRate);
            const float local = t * 2.0f - static_cast<float>(note);
            value = std::sin(phase) * envelope(local, 0.1f) * 0.38f;
            break;
        }
        case Cue::Count:
            break;
        }

        samples[static_cast<std::size_t>(i)] = toSample(value);
    }
    return samples;
}

bool Audio::selfTest()
{
    // 1. The cue table is complete and every cue is non-silent, so a cue that
    //    failed to synthesise cannot hide behind a silent buffer.
    std::vector<CueSamples> table;
    table.reserve(kCueCount);
    for (std::size_t i = 0; i < kCueCount; ++i)
    {
        CueSamples baked;
        baked.samples = synthesise(static_cast<Cue>(i));
        if (baked.samples.empty())
        {
            std::fprintf(stderr, "audio self-test: cue %zu is empty\n", i);
            return false;
        }
        bool nonSilent = false;
        for (std::int16_t s : baked.samples)
        {
            if (s != 0)
            {
                nonSilent = true;
                break;
            }
        }
        if (!nonSilent)
        {
            std::fprintf(stderr, "audio self-test: cue %zu is silent\n", i);
            return false;
        }
        table.push_back(std::move(baked));
    }

    // 2. Synthesis is deterministic: a second bake is bit-for-bit identical, so
    //    a cue does not depend on time or global state.
    for (std::size_t i = 0; i < kCueCount; ++i)
    {
        if (synthesise(static_cast<Cue>(i)) != table[i].samples)
        {
            std::fprintf(stderr, "audio self-test: cue %zu is not deterministic\n", i);
            return false;
        }
    }

    // 3. Mixing sums active cues. A voice survives a chunk shorter than its cue
    //    and is dropped once it runs past the end, and two copies of a cue read
    //    louder than one (measured by peak, since sample 0 is a zero-crossing).
    Audio mixer;
    mixer.cues_ = table;
    const std::size_t hitLen = table[static_cast<std::size_t>(Cue::Hit)].length();

    auto peak = [](const std::vector<std::int16_t>& buffer) {
        int m = 0;
        for (std::int16_t s : buffer)
        {
            m = std::max(m, std::abs(static_cast<int>(s)));
        }
        return m;
    };

    std::vector<std::int16_t> one(kChunkFrames, 0);
    mixer.voices_.push_back(Voice{static_cast<std::size_t>(Cue::Hit), 0});
    mixer.mixInto(one);
    if (hitLen > kChunkFrames && mixer.voices_.empty())
    {
        std::fprintf(stderr, "audio self-test: a cue drained before its end\n");
        return false;
    }

    std::vector<std::int16_t> two(kChunkFrames, 0);
    mixer.voices_.clear();
    mixer.voices_.push_back(Voice{static_cast<std::size_t>(Cue::Hit), 0});
    mixer.voices_.push_back(Voice{static_cast<std::size_t>(Cue::Hit), 0});
    mixer.mixInto(two);
    if (peak(two) <= peak(one))
    {
        std::fprintf(stderr, "audio self-test: mixing did not sum cues (%d vs %d)\n",
                     peak(two), peak(one));
        return false;
    }

    // A voice is dropped once it has played past the end of its cue.
    mixer.voices_.clear();
    mixer.voices_.push_back(Voice{static_cast<std::size_t>(Cue::Hit), 0});
    for (std::size_t played = 0; played <= hitLen + kChunkFrames; played += kChunkFrames)
    {
        mixer.mixInto(one);
    }
    if (!mixer.voices_.empty())
    {
        std::fprintf(stderr, "audio self-test: a finished voice was not dropped\n");
        return false;
    }

    // 4. A disabled Audio is a safe no-op: play/update must not touch a device.
    Audio silent;
    if (silent.enabled())
    {
        std::fprintf(stderr, "audio self-test: an un-inited Audio reports enabled\n");
        return false;
    }
    silent.play(Cue::Shoot);
    silent.update();
    silent.shutdown();
    silent.shutdown(); // idempotent

    // 5. The outcome mapping is total and distinct.
    if (cueForState(core::MissionState::Won) == cueForState(core::MissionState::Lost))
    {
        std::fprintf(stderr, "audio self-test: won and lost map to the same cue\n");
        return false;
    }

    // 6. The event->cue mapping fires on a *change*, not on a steady state, so a
    //    quiet frame is silent and a kill/outcome fires exactly once. This is the
    //    real decision `Game::updateAudio` uses, so the guard covers the wiring.
    auto contains = [](const std::vector<Cue>& cues, Cue cue) {
        return std::find(cues.begin(), cues.end(), cue) != cues.end();
    };

    AudioEvents prev; // all zero, Running, no shot
    if (!cuesForFrame(prev, prev).empty())
    {
        std::fprintf(stderr, "audio self-test: a steady frame fired a cue\n");
        return false;
    }

    AudioEvents shot = prev;
    shot.anyShot = true;
    if (!contains(cuesForFrame(prev, shot), Cue::Shoot))
    {
        std::fprintf(stderr, "audio self-test: a shot did not fire the Shoot cue\n");
        return false;
    }

    AudioEvents kill = prev;
    kill.enemyKills = 1;
    const std::vector<Cue> killCues = cuesForFrame(prev, kill);
    if (!contains(killCues, Cue::Hit) || contains(killCues, Cue::Explosion))
    {
        std::fprintf(stderr, "audio self-test: a combat kill mapped to the wrong cue\n");
        return false;
    }
    // The next frame (same counts) must be silent: the kill fired once.
    if (!cuesForFrame(kill, kill).empty())
    {
        std::fprintf(stderr, "audio self-test: a kill fired more than once\n");
        return false;
    }

    AudioEvents collateral = prev;
    collateral.civilianKills = 1;
    if (!contains(cuesForFrame(prev, collateral), Cue::Explosion))
    {
        std::fprintf(stderr, "audio self-test: a civilian kill did not fire Explosion\n");
        return false;
    }

    AudioEvents won = prev;
    won.state = core::MissionState::Won;
    const std::vector<Cue> wonCues = cuesForFrame(prev, won);
    if (!contains(wonCues, Cue::Victory))
    {
        std::fprintf(stderr, "audio self-test: winning did not fire Victory\n");
        return false;
    }
    // A decided state is steady: it must not fire again on later frames.
    if (!cuesForFrame(won, won).empty())
    {
        std::fprintf(stderr, "audio self-test: the outcome fired more than once\n");
        return false;
    }

    AudioEvents lost = prev;
    lost.state = core::MissionState::Lost;
    if (!contains(cuesForFrame(prev, lost), Cue::Defeat))
    {
        std::fprintf(stderr, "audio self-test: losing did not fire Defeat\n");
        return false;
    }

    // 7. The UI action mapping is total, injective, and never a combat cue
    //    (issue #424): a selection, a menu move, and a cancel must each map to
    //    their own UI blip, so a menu key cannot be mistaken for a shot.
    const Cue selectCue = cueForUiAction(UiAction::Select);
    const Cue navigateCue = cueForUiAction(UiAction::Navigate);
    const Cue cancelCue = cueForUiAction(UiAction::Cancel);
    if (selectCue == navigateCue || selectCue == cancelCue || navigateCue == cancelCue)
    {
        std::fprintf(stderr, "audio self-test: UI actions map to the same cue\n");
        return false;
    }
    const Cue combatCues[] = {Cue::Shoot, Cue::Hit, Cue::Explosion, Cue::Victory,
                              Cue::Defeat};
    for (Cue ui : {selectCue, navigateCue, cancelCue})
    {
        for (Cue combat : combatCues)
        {
            if (ui == combat)
            {
                std::fprintf(stderr, "audio self-test: a UI action mapped to a combat cue\n");
                return false;
            }
        }
    }
    // The UI cues are baked and non-silent like every other cue (step 1 checked
    // the whole table, including them), so a menu key is actually audible.

    return true;
}

} // namespace fossynd::game
