module;

#include <cmath>
#include <cstdint>
#include <vector>

export module Artifact.Acoustic;

export namespace Artifact::Acoustic {

    // 物理定数
    export constexpr float SPEED_OF_SOUND = 343.0f; // m/s
    export constexpr float AIR_DENSITY = 1.225f;    // kg/m3

    // 音響タスクの型
    export enum class SynthesisType {
        Modal,      // インパクト音 (サイン波合成)
        Stochastic, // 雨・砂など (ショットノイズ/粒状合成)
        Flow,       // 風・気流 (ノイズフィルタリング)
        Friction    // 摩擦 (テクスチャードノイズ)
    };

    // 簡易的なベクトル構造体
    export struct Vector3 {
        float x, y, z;
        float Length() const { return std::sqrt(x*x + y*y + z*z); }
        Vector3 operator-(const Vector3& other) const { return {x-other.x, y-other.y, z-other.z}; }
        float Dot(const Vector3& other) const { return x*other.x + y*other.y + z*other.z; }
    };

    // レンダラーへの詳細指示書
    export struct AudioTask {
        SynthesisType type;
        float amplitude;
        float frequency;
        float qFactor;
        float duration;
        float pan;          // -1.0(左) ~ 1.0(右)
        float doppler;      // 周波数倍率 (1.0 = 変化なし)
        float attenuation;  // 距離による減衰 (0.0~1.0)
        std::uint32_t seed;
    };

    // 音響合成タスクのデバッグ用情報
    export struct AudioTaskDebug {
        std::uint32_t layerId;
        SynthesisType type;
        float freq;
        float amp;
        float duration;
        float attenuation;
    };

    // 1フレーム分の音響情報のスナップショット
    export struct AcousticSnapshot {
        std::uint64_t frameNumber;
        double timestamp;
        std::vector<AudioTaskDebug> activeTasks;
        std::vector<AudioTaskDebug> culledTasks; // LODで間引かれたタスク
    };

    // モデルの基本インターフェース
    export class IAcousticModel {
    public:
        virtual ~IAcousticModel() = default;
        virtual void Update(float dt) = 0;
        virtual void Trigger(float impulse, float position) {} // デフォルトでは何もしない
        virtual std::vector<AudioTask> GenerateTasks() = 0;
    };
}
