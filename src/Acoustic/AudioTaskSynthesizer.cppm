module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

export module Artifact.Acoustic.AudioTaskSynthesizer;

import Artifact.Acoustic;

export namespace Artifact::Acoustic {

    // Converts the bounded task set produced by AcousticSystem into caller-owned
    // interleaved stereo PCM. The renderer itself does not allocate.
    export class AudioTaskSynthesizer {
    public:
        static constexpr std::size_t MaxTasks = 32;

        AudioTaskSynthesizer() { Reset(); }

        bool RenderStereo(const AudioTaskBlock& taskBlock,
                          std::span<float> interleavedOutput,
                          int sampleRate) {
            return RenderStereo(taskBlock.view(), interleavedOutput, sampleRate);
        }

        bool RenderStereo(std::span<const AudioTask> tasks,
                          std::span<float> interleavedOutput,
                          int sampleRate) {
            if ((interleavedOutput.size() % 2) != 0) {
                return false;
            }
            return render(tasks, interleavedOutput.data(), nullptr, nullptr,
                          interleavedOutput.size() / 2, sampleRate, false);
        }

        bool RenderPlanarAdd(const AudioTaskBlock& taskBlock,
                             std::span<float> leftOutput,
                             std::span<float> rightOutput,
                             int sampleRate) {
            return RenderPlanarAdd(taskBlock.view(), leftOutput, rightOutput, sampleRate);
        }

        bool RenderPlanarAdd(std::span<const AudioTask> tasks,
                             std::span<float> leftOutput,
                             std::span<float> rightOutput,
                             int sampleRate) {
            if (leftOutput.size() != rightOutput.size()) return false;
            return render(tasks, nullptr, leftOutput.data(), rightOutput.data(),
                          leftOutput.size(), sampleRate, true);
        }

        void Reset() {
            for (std::size_t i = 0; i < states_.size(); ++i) {
                states_[i] = State{};
                states_[i].randomState = 0x9e3779b9u ^ static_cast<std::uint32_t>(i + 1);
            }
        }

    private:
        bool render(std::span<const AudioTask> tasks,
                    float* interleavedOutput,
                    float* leftOutput,
                    float* rightOutput,
                    std::size_t frames,
                    int sampleRate,
                    bool accumulate) {
            if (sampleRate < 8000 || sampleRate > 384000 || tasks.size() > MaxTasks) {
                return false;
            }
            if (!accumulate && interleavedOutput) {
                std::fill(interleavedOutput, interleavedOutput + frames * 2, 0.0f);
            }
            const float rate = static_cast<float>(sampleRate);
            const float attackSmoothing = 1.0f - std::exp(-1.0f / (0.005f * rate));
            const float releaseSmoothing = 1.0f - std::exp(-1.0f / (0.03f * rate));
            for (std::size_t i = 0; i < MaxTasks; ++i) {
                State& state = states_[i];
                state.targetGain = i < tasks.size() ? taskGain(tasks[i]) : 0.0f;
                if (i < tasks.size()) {
                    const AudioTask& task = tasks[i];
                    const float frequency = std::clamp(
                        finiteOr(task.frequency, 440.0f), 1.0f, rate * 0.45f);
                    const float doppler = std::clamp(
                        finiteOr(task.doppler, 1.0f), 0.25f, 4.0f);
                    const float qFactor = std::clamp(
                        finiteOr(task.qFactor, 1.0f), 0.1f, 1000.0f);
                    state.phaseStep = frequency * doppler / rate;
                    if (task.type == SynthesisType::Flow) {
                        // Flow tasks describe a spectral corner in Hz; Q is not a
                        // meaningful low-pass divisor for broadband environmental noise.
                        const float cutoffHz = std::clamp(frequency * 4.0f, 80.0f, 10000.0f);
                        state.noiseCutoff = 1.0f - std::exp(-kTau * cutoffHz / rate);
                    } else if (task.type == SynthesisType::BandPassNoise) {
                        const float bandwidth = std::max(
                            80.0f, frequency / std::clamp(qFactor, 0.5f, 8.0f));
                        const float lowEdge = std::max(30.0f, frequency - bandwidth * 0.5f);
                        const float highEdge = std::clamp(
                            frequency + bandwidth * 0.5f, lowEdge + 40.0f, rate * 0.45f);
                        state.noiseCutoff = 1.0f - std::exp(-kTau * lowEdge / rate);
                        state.bandCutoff = 1.0f - std::exp(-kTau * highEdge / rate);
                    } else {
                        state.noiseCutoff = std::clamp(frequency / (qFactor * rate),
                            0.001f, 0.45f);
                    }
                    state.type = task.type;
                    state.dropEventRate = state.type == SynthesisType::Droplet
                        ? std::clamp(finiteOr(task.eventRate, 0.0f), 0.0f, 4000.0f)
                        : 0.0f;
                    state.dropPanSpread = state.type == SynthesisType::Droplet
                        ? std::clamp(finiteOr(task.panSpread, 0.0f), 0.0f, 1.0f)
                        : 0.0f;
                    state.dropDecay = std::exp(-1.0f / (std::clamp(
                        finiteOr(task.duration, 0.02f), 0.003f, 0.1f) * rate));
                    state.dropFrequency = frequency * doppler;
                    const float pan = std::clamp(finiteOr(task.pan, 0.0f), -1.0f, 1.0f);
                    state.leftPan = std::sqrt((1.0f - pan) * 0.5f);
                    state.rightPan = std::sqrt((1.0f + pan) * 0.5f);
                    if (!state.seedInitialized) {
                        state.randomState = task.seed != 0
                            ? task.seed
                            : (0x9e3779b9u ^ static_cast<std::uint32_t>(i + 1));
                        state.seedInitialized = true;
                    }
                } else {
                    state.leftPan = 0.70710678f;
                    state.rightPan = 0.70710678f;
                }
            }

            for (std::size_t frame = 0; frame < frames; ++frame) {
                float left = 0.0f;
                float right = 0.0f;
                for (std::size_t taskIndex = 0; taskIndex < MaxTasks; ++taskIndex) {
                    State& state = states_[taskIndex];
                    const bool active = taskIndex < tasks.size();
                    const float target = active ? state.targetGain : 0.0f;
                    const float smoothing = target > state.gain
                        ? attackSmoothing : releaseSmoothing;
                    state.gain += (target - state.gain) * smoothing;
                    if (state.gain < 0.00001f) continue;

                    if (state.type == SynthesisType::Droplet) {
                        renderDroplets(state, taskIndex < tasks.size(), rate,
                                       state.gain, left, right);
                        continue;
                    }

                    float sample = 0.0f;
                    if (active) {
                        switch (state.type) {
                            case SynthesisType::Modal:
                                sample = std::sin(state.phase * kTau);
                                state.phase = wrapPhase(state.phase + state.phaseStep);
                                break;
                            case SynthesisType::Stochastic:
                            case SynthesisType::Flow:
                            case SynthesisType::BandPassNoise:
                            case SynthesisType::Friction: {
                                const float noise = nextNoise(state.randomState);
                                state.lowPass += state.noiseCutoff * (noise - state.lowPass);
                                if (state.type == SynthesisType::Flow) {
                                    sample = state.lowPass * 0.82f + noise * 0.18f;
                                }
                                else if (state.type == SynthesisType::BandPassNoise) {
                                    const float highPassed = noise - state.lowPass;
                                    state.bandPass += state.bandCutoff * (highPassed - state.bandPass);
                                    sample = state.bandPass;
                                }
                                else if (state.type == SynthesisType::Friction) sample = noise - state.lowPass;
                                else sample = state.lowPass * 0.65f + noise * 0.35f;
                                break;
                            }
                            case SynthesisType::Droplet: break;
                        }
                    }

                    const float rendered = sample * state.gain;
                    left += rendered * state.leftPan;
                    right += rendered * state.rightPan;
                }
                if (interleavedOutput) {
                    interleavedOutput[frame * 2] = std::clamp(left, -1.0f, 1.0f);
                    interleavedOutput[frame * 2 + 1] = std::clamp(right, -1.0f, 1.0f);
                } else if (accumulate) {
                    leftOutput[frame] += left;
                    rightOutput[frame] += right;
                } else {
                    leftOutput[frame] = std::clamp(left, -1.0f, 1.0f);
                    rightOutput[frame] = std::clamp(right, -1.0f, 1.0f);
                }
            }
            return true;
        }
        struct State {
            struct DropGrain {
                float lowPass = 0.0f;
                float filterCoefficient = 0.1f;
                float impactPhase = 0.0f;
                float impactPhaseStep = 0.01f;
                float impactEnvelope = 0.0f;
                float impactDecay = 0.9f;
                float impactLevel = 0.0f;
                float leftPan = 0.70710678f;
                float rightPan = 0.70710678f;
                float envelope = 0.0f;
                float decay = 0.99f;
                float level = 0.0f;
            };

            float phase = 0.0f;
            float lowPass = 0.0f;
            float gain = 0.0f;
            float targetGain = 0.0f;
            float phaseStep = 0.0f;
            float noiseCutoff = 0.01f;
            float bandCutoff = 0.1f;
            float bandPass = 0.0f;
            float leftPan = 0.70710678f;
            float rightPan = 0.70710678f;
            float dropEventRate = 0.0f;
            float dropPanSpread = 0.0f;
            float dropCountdown = 0.0f;
            float dropDecay = 0.99f;
            float dropFrequency = 700.0f;
            std::array<DropGrain, 16> dropGrains{};
            std::size_t nextDropGrain = 0;
            SynthesisType type = SynthesisType::Modal;
            std::uint32_t randomState = 1;
            bool seedInitialized = false;
        };

        static constexpr float kTau = 6.28318530718f;

        static float finiteOr(float value, float fallback) {
            return std::isfinite(value) ? value : fallback;
        }

        static float taskGain(const AudioTask& task) {
            const float amplitude = std::clamp(finiteOr(task.amplitude, 0.0f), 0.0f, 1.0f);
            const float attenuation = std::clamp(finiteOr(task.attenuation, 1.0f), 0.0f, 1.0f);
            return amplitude * attenuation;
        }

        static float wrapPhase(float phase) {
            return phase - std::floor(phase);
        }

        void renderDroplets(State& state,
                            bool emitNewDrops,
                            float sampleRate,
                            float gain,
                            float& leftOutput,
                            float& rightOutput) {
            if (emitNewDrops && state.dropEventRate > 0.0f) {
                state.dropCountdown -= 1.0f;
                int spawned = 0;
                while (state.dropCountdown <= 0.0f && spawned < 8) {
                    auto& grain = state.dropGrains[state.nextDropGrain];
                    state.nextDropGrain = (state.nextDropGrain + 1) % state.dropGrains.size();

                    const float randomLevel = (nextNoise(state.randomState) + 1.0f) * 0.5f;
                    const float randomColor = (nextNoise(state.randomState) + 1.0f) * 0.5f;
                    const float randomImpact = (nextNoise(state.randomState) + 1.0f) * 0.5f;
                    const float randomPhase = (nextNoise(state.randomState) + 1.0f) * 0.5f;
                    const float randomPan = (nextNoise(state.randomState) + 1.0f) * 0.5f;
                    const float randomInterval = std::max(
                        0.000001f, (nextNoise(state.randomState) + 1.0f) * 0.5f);
                    const float cutoffFrequency = std::clamp(
                        state.dropFrequency * (1.0f + randomColor * 2.0f),
                        400.0f, sampleRate * 0.4f);
                    grain.lowPass = 0.0f;
                    grain.filterCoefficient = 1.0f - std::exp(
                        -kTau * cutoffFrequency / sampleRate);
                    grain.envelope = 1.0f;
                    grain.decay = std::pow(state.dropDecay, 0.75f + randomLevel * 0.5f);
                    grain.level = 0.08f * (0.25f + randomLevel * 0.75f);
                    const float impactFrequency = 1000.0f + randomImpact * 15000.0f;
                    grain.impactPhase = randomPhase;
                    grain.impactPhaseStep = impactFrequency / sampleRate;
                    grain.impactEnvelope = 1.0f;
                    grain.impactDecay = std::exp(-2.0f * impactFrequency / sampleRate);
                    grain.impactLevel = 0.035f * (0.5f + randomLevel * 0.5f);
                    const float pan = (randomPan * 2.0f - 1.0f) * state.dropPanSpread;
                    grain.leftPan = std::sqrt((1.0f - pan) * 0.5f);
                    grain.rightPan = std::sqrt((1.0f + pan) * 0.5f);

                    state.dropCountdown += -std::log(randomInterval) *
                        sampleRate / state.dropEventRate;
                    ++spawned;
                }
            }

            for (auto& grain : state.dropGrains) {
                if (grain.envelope < 0.001f) continue;
                const float noise = nextNoise(state.randomState);
                grain.lowPass += grain.filterCoefficient * (noise - grain.lowPass);
                const float brightTransient = noise - grain.lowPass;
                const float impact = std::sin(grain.impactPhase * kTau) *
                    grain.impactEnvelope * grain.impactLevel;
                const float output = ((grain.lowPass * 0.35f + brightTransient * 0.65f) *
                    grain.envelope * grain.level + impact) * gain;
                leftOutput += output * grain.leftPan;
                rightOutput += output * grain.rightPan;
                grain.impactPhase = wrapPhase(
                    grain.impactPhase + grain.impactPhaseStep);
                grain.impactEnvelope *= grain.impactDecay;
                grain.envelope *= grain.decay;
            }
        }

        static float nextNoise(std::uint32_t& state) {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return static_cast<float>(state & 0x00ffffffu) * (2.0f / 16777215.0f) - 1.0f;
        }

        std::array<State, MaxTasks> states_{};
    };
}
