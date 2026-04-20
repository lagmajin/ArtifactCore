export module Artifact.Acoustic.RainModel;

import Artifact.Acoustic;
#include <algorithm>
#include <cstdint>
#include <vector>

export namespace Artifact::Acoustic {

    export class RainModel : public IAcousticModel {
    public:
        void SetIntensity(float dropsPerSecond) {
            m_intensity = std::clamp(dropsPerSecond, 0.0f, 10000.0f);
        }

        void SetDropSize(float size) {
            m_dropSize = std::clamp(size, 0.1f, 5.0f);
        }

        void Update(float dt) override {
            // 強度やサイズの時間的変化があればここで計算
        }

        std::vector<AudioTask> GenerateTasks() override {
            std::vector<AudioTask> tasks;
            
            // 雨の強さに応じて、統計的なタスクを発行
            // 実際にはGPU側で1粒単位の合成を行うが、
            // ここではその「統計的な包絡線」を渡す
            if (m_intensity > 0.0f) {
                tasks.push_back({
                    SynthesisType::Stochastic,
                    m_intensity / 1000.0f,   // Amplitude
                    500.0f + (m_dropSize * 200.0f), // Frequency (音の太さ)
                    1.0f,                    // Q Factor
                    0.1f,                    // Duration (短期的な更新)
                    0.0f,                    // Pan
                    1.0f,                    // Doppler
                    static_cast<std::uint32_t>(m_intensity * 1234.5f) // Seed
                });
            }
            return tasks;
        }

    private:
        float m_intensity = 0.0f;
        float m_dropSize = 1.0f;
    };
}
