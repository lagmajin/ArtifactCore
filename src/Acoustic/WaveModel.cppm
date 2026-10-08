module;

#include <algorithm>
#include <cmath>
#include <cstdint>

export module Artifact.Acoustic.WaveModel;

import Artifact.Acoustic;

export namespace Artifact::Acoustic {

    // Generates a repeating water-swell bed and a short stochastic breaking transient.
    export class WaveModel : public IAcousticModel {
    public:
        void SetWaveHeight(float normalizedHeight) {
            if (std::isfinite(normalizedHeight)) {
                m_height = std::clamp(normalizedHeight, 0.0f, 1.0f);
            }
        }

        void SetPeriod(float seconds) {
            if (std::isfinite(seconds)) {
                m_period = std::clamp(seconds, 0.5f, 60.0f);
                m_cyclePeriod = m_period;
            }
        }

        void SetBreaking(float normalizedIntensity) {
            if (std::isfinite(normalizedIntensity)) {
                m_breaking = std::clamp(normalizedIntensity, 0.0f, 1.0f);
            }
        }

        void Update(float dt) override {
            if (!std::isfinite(dt) || dt <= 0.0f) return;
            m_phase += dt / m_cyclePeriod;
            if (m_phase >= 1.0f) {
                m_phase -= std::floor(m_phase);
                const float variation = static_cast<float>(nextRandom() & 0xffffu) / 32767.5f - 1.0f;
                m_cyclePeriod = std::clamp(m_period * (1.0f + variation * 0.08f), 0.5f, 60.0f);
            }
        }

        AudioTaskBatch GenerateTasks() override {
            AudioTaskBatch tasks;

            if (m_height <= 0.0f) return tasks;

            constexpr float tau = 6.28318530718f;
            const float swell = 0.5f - 0.5f * std::cos(tau * m_phase);
            const float surgeAmplitude = m_height * (0.25f + 0.75f * swell);
            tasks.append({
                SynthesisType::Flow,
                surgeAmplitude * 0.45f,
                std::clamp(70.0f + m_height * 180.0f, 20.0f, 1200.0f),
                1.2f,
                std::clamp(m_period * 0.2f, 0.08f, 1.0f),
                0.0f,
                1.0f,
                1.0f,
                0
            });

            // The break crests near the end of each swell; circular distance keeps
            // the envelope continuous across the phase wrap.
            constexpr float breakPhase = 0.78f;
            const float phaseDistance = std::min(
                std::abs(m_phase - breakPhase), 1.0f - std::abs(m_phase - breakPhase));
            const float breakEnvelope = std::exp(-phaseDistance * phaseDistance / 0.0032f);
            const float breakAmplitude = m_height * m_breaking * breakEnvelope;
            if (breakAmplitude > 0.001f) {
                tasks.append({
                    SynthesisType::Stochastic,
                    breakAmplitude * 0.7f,
                    900.0f + 3200.0f * m_height,
                    0.8f,
                    0.12f + 0.28f * m_height,
                    0.0f,
                    1.0f,
                    1.0f,
                    static_cast<std::uint32_t>(m_phase * 65535.0f)
                });
            }

            return tasks;
        }

    private:
        std::uint32_t nextRandom() {
            m_randomState ^= m_randomState << 13;
            m_randomState ^= m_randomState >> 17;
            m_randomState ^= m_randomState << 5;
            return m_randomState;
        }

        float m_height = 0.0f;
        float m_period = 6.0f;
        float m_cyclePeriod = 6.0f;
        float m_breaking = 0.65f;
        float m_phase = 0.0f;
        std::uint32_t m_randomState = 0xa341316cu;
    };
}
