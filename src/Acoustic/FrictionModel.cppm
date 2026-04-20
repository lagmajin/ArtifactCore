export module Artifact.Acoustic.FrictionModel;

import Artifact.Acoustic;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

export namespace Artifact::Acoustic {

    export class FrictionModel : public IAcousticModel {
    public:
        void SetRelativeVelocity(float v) { m_velocity = std::abs(v); }
        void SetPressure(float p) { m_pressure = std::clamp(p, 0.0f, 10.0f); }
        void SetRoughness(float r) { m_roughness = std::clamp(r, 0.0f, 1.0f); }

        void Update(float dt) override {
            // 速度に応じて摩擦の「激しさ」をエンベロープ化
            m_intensity = std::lerp(m_intensity, m_velocity * m_pressure, 0.1f);
        }

        std::vector<AudioTask> GenerateTasks() override {
            std::vector<AudioTask> tasks;
            
            if (m_intensity > 0.01f) {
                // 摩擦音は「高域ノイズの帯域制限」としてモデル化
                // 速度が上がるほどピッチ（中心周波数）と鋭さ（Q）が上がる
                float centerFreq = 1000.0f + (m_velocity * 500.0f) + (m_roughness * 2000.0f);
                
                tasks.push_back({
                    SynthesisType::Friction,
                    std::min(1.0f, m_intensity * 0.2f),
                    centerFreq,
                    2.0f + (m_velocity * 0.5f), // 速度で摩擦の「鋭さ」が変わる
                    0.1f,
                    0.0f,
                    1.0f,
                    static_cast<std::uint32_t>(m_velocity * 1000.0f)
                });
            }
            return tasks;
        }

    private:
        float m_velocity = 0.0f;
        float m_pressure = 1.0f;
        float m_roughness = 0.5f;
        float m_intensity = 0.0f;
    };
}
