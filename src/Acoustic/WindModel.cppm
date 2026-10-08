module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>

export module Artifact.Acoustic.WindModel;

import Artifact.Acoustic;

export namespace Artifact::Acoustic {

    export class WindModel : public IAcousticModel {
    public:
        void SetVelocity(float velocity_ms) {
            m_velocity = std::isfinite(velocity_ms)
                ? std::clamp(velocity_ms, 0.0f, 80.0f) : 0.0f;
        }

        void SetObstacleDiameter(float diameter_m) {
            m_diameter = std::max(0.01f, diameter_m);
        }

        void Update(float dt) override {
            if (!std::isfinite(dt) || dt <= 0.0f) return;
            const float randomTarget = static_cast<float>(nextRandom() & 0xffffu) / 32767.5f - 1.0f;
            const float timeConstant = randomTarget > m_gust ? 0.22f : 0.9f;
            const float smoothing = 1.0f - std::exp(-dt / timeConstant);
            m_gust += (randomTarget - m_gust) * smoothing;
        }

        AudioTaskBatch GenerateTasks() override {
            AudioTaskBatch tasks;

            if (m_velocity > 0.5f) {
                // Verron & Drettakis model wind as several independently
                // modulated band-limited noise atoms, rather than one filtered bed.
                constexpr float centersHz[] = { 150.0f, 430.0f, 1250.0f, 3600.0f };
                constexpr float bandGains[] = { 0.50f, 0.38f, 0.26f, 0.15f };
                const float speedRatio = std::clamp(m_velocity / 8.0f, 0.35f, 2.5f);
                const float gustGain = 0.72f + 0.28f * (m_gust + 1.0f) * 0.5f;
                const float baseAmplitude = std::min(1.0f, m_velocity * 0.05f) * gustGain;
                for (std::size_t i = 0; i < std::size(centersHz); ++i) {
                    const float center = std::clamp(
                        centersHz[i] * speedRatio, 60.0f, 10000.0f);
                    tasks.append({
                        SynthesisType::BandPassNoise,
                        baseAmplitude * bandGains[i],
                        center,
                        0.9f, // Broad, overlapping bands for wind noise.
                        0.16f,
                        (static_cast<float>(i) - 1.5f) * 0.08f,
                        1.0f,
                        1.0f,
                        0x6d2b79f5u ^ static_cast<std::uint32_t>(i + 1)
                    });
                }
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

        float m_velocity = 0.0f;
        float m_diameter = 0.1f; // 10cmの棒に当たっていると仮定
        float m_gust = 0.0f;
        std::uint32_t m_randomState = 0x6d2b79f5u;
    };
}
