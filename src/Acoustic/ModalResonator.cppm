module;

#include <algorithm>
#include <cmath>
#include <vector>

export module Artifact.Acoustic.ModalResonator;

import Artifact.Acoustic;

export namespace Artifact::Acoustic {

    export struct MaterialProfile {
        float density;
        float stiffness;
        float damping;
    };

    export class ModalResonator : public IAcousticModel {
    public:
        struct Mode {
            float freq;
            float decay;
            float currentAmp;
        };

        ModalResonator(const MaterialProfile& profile) : m_profile(profile) {
            InitializeModes();
        }

        void Trigger(float impulse, float position) {
            for (auto& mode : m_modes) {
                float excitation = std::abs(std::sin(mode.freq * 0.001f * position));
                mode.currentAmp = std::min(1.0f, mode.currentAmp + (impulse * excitation * m_profile.stiffness));
            }
        }

        void Update(float dt) override {
            for (auto& mode : m_modes) {
                mode.currentAmp *= std::exp(-mode.decay * m_profile.damping * dt);
            }
        }

        std::vector<AudioTask> GenerateTasks() override {
            std::vector<AudioTask> tasks;
            for (const auto& mode : m_modes) {
                if (mode.currentAmp > 0.0001f) {
                    tasks.push_back({
                        SynthesisType::Modal,
                        mode.currentAmp,
                        mode.freq,
                        100.0f, // Q Factor (高いほど純音に近い)
                        1.0f / (mode.decay * m_profile.damping),
                        0.0f,
                        1.0f,
                        mode.currentAmp,
                        0
                    });
                }
            }
            return tasks;
        }

    private:
        void InitializeModes() {
            float base = 220.0f / (m_profile.density + 0.1f);
            for (int i = 1; i <= 8; ++i) {
                m_modes.push_back({ 
                    base * std::pow((float)i, 1.2f), // 非調和倍音
                    (float)i * 0.5f, 
                    0.0f 
                });
            }
        }

        MaterialProfile m_profile;
        std::vector<Mode> m_modes;
    };
}
