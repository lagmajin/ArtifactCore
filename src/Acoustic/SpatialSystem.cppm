module;

#include <algorithm>
#include <cmath>

export module Artifact.Acoustic.Spatial;

import Artifact.Acoustic;

export namespace Artifact::Acoustic {

    export struct SpatialState {
        Vector3 position;
        Vector3 velocity;
    };

    export class SpatialCalculator {
    public:
        void SetListener(const SpatialState& listener) { m_listener = listener; }

        // 音源の状態から、パン、減衰、ドップラー係数を計算
        void Calculate(const SpatialState& source, AudioTask& task) {
            Vector3 relativePos = source.position - m_listener.position;
            float distance = relativePos.Length();
            
            // 1. 距離減衰 (Inverse Square Law 簡易版)
            task.attenuation = 1.0f / (1.0f + 0.1f * distance * distance);
            
            // 2. パン (左右)
            // カメラの向きを考慮する必要があるが、ここでは簡易的にX軸の差で計算
            if (distance > 0.001f) {
                task.pan = std::clamp(relativePos.x / distance, -1.0f, 1.0f);
            }

            // 3. ドップラー効果
            // f' = f * (v_sound + v_listener) / (v_sound - v_source)
            Vector3 unitPos = { relativePos.x / distance, relativePos.y / distance, relativePos.z / distance };
            float v_l = m_listener.velocity.Dot(unitPos);
            float v_s = source.velocity.Dot(unitPos);
            
            task.doppler = (SPEED_OF_SOUND + v_l) / (SPEED_OF_SOUND - v_s);
            task.doppler = std::clamp(task.doppler, 0.5f, 2.0f); // 極端な変化を抑制
        }

    private:
        SpatialState m_listener;
    };
}
