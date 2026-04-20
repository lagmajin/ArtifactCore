export module Artifact.Acoustic.WindModel;

import Artifact.Acoustic;
#include <algorithm>
#include <cmath>
#include <vector>

export namespace Artifact::Acoustic {

    export class WindModel : public IAcousticModel {
    public:
        void SetVelocity(float velocity_ms) {
            m_velocity = velocity_ms;
        }

        void SetObstacleDiameter(float diameter_m) {
            m_diameter = std::max(0.01f, diameter_m);
        }

        void Update(float dt) override {
            // 風速の揺らぎ（ガスト）をシミュレート可能
        }

        std::vector<AudioTask> GenerateTasks() override {
            std::vector<AudioTask> tasks;
            
            // ストローハル数 (St ~ 0.2) を用いた中心周波数の計算
            // f = St * V / D
            float frequency = 0.2f * m_velocity / m_diameter;
            
            // 可聴域に制限
            frequency = std::clamp(frequency, 20.0f, 15000.0f);

            if (m_velocity > 0.5f) {
                tasks.push_back({
                    SynthesisType::Flow,
                    std::min(1.0f, m_velocity * 0.05f), // Amplitude
                    frequency,
                    5.0f + (m_velocity * 0.2f),       // Q Factor (速いほど鋭い音)
                    0.16f,                             // Duration
                    0.0f,
                    1.0f,
                    0
                });
            }
            return tasks;
        }

    private:
        float m_velocity = 0.0f;
        float m_diameter = 0.1f; // 10cmの棒に当たっていると仮定
    };
}
