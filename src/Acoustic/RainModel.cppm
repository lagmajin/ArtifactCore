module;
#include <algorithm>
#include <cstdint>

export module Artifact.Acoustic.RainModel;

import Artifact.Acoustic;

export namespace Artifact::Acoustic {

    export class RainModel : public IAcousticModel {
    public:
        void SetIntensity(float dropsPerSecond) {
            m_intensity = std::clamp(dropsPerSecond, 0.0f, 10000.0f);
        }

        void SetDropSize(float size) {
            m_dropSize = std::clamp(size, 0.1f, 5.0f);
        }

        void SetWaterImpact(bool enabled) {
            m_waterImpact = enabled;
        }

        void Update(float dt) override {
            // 強度やサイズの時間的変化があればここで計算
        }

        AudioTaskBatch GenerateTasks() override {
            AudioTaskBatch tasks;
            
            // 雨の強さに応じて、統計的なタスクを発行
            // 実際にはGPU側で1粒単位の合成を行うが、
            // ここではその「統計的な包絡線」を渡す
            if (m_intensity > 0.0f) {
                tasks.append({
                    SynthesisType::Droplet,
                    std::clamp(m_intensity / 1000.0f, 0.0f, 1.0f), // Output gain
                    500.0f + (m_dropSize * 200.0f), // Resonant drop frequency
                    1.0f,                    // Unused Q factor
                    0.008f + m_dropSize * 0.008f, // Drop decay time
                    0.0f,                    // Pan
                    1.0f,                    // Doppler
                    1.0f,
                    static_cast<std::uint32_t>(m_intensity * 1234.5f), // Seed
                    std::min(m_intensity, 4000.0f), // Independent drop rate
                    0.85f, // Spread individual drops across the stereo field
                    m_waterImpact
                });
            }
            return tasks;
        }

    private:
        float m_intensity = 0.0f;
        float m_dropSize = 1.0f;
        bool m_waterImpact = false;
    };
}
