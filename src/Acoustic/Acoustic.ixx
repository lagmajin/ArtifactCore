module;

#include <cmath>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

export module Artifact.Acoustic;

export import Container.NamedVector;

export namespace Artifact::Acoustic {

    // 物理定数
    export constexpr float SPEED_OF_SOUND = 343.0f; // m/s
    export constexpr float AIR_DENSITY = 1.225f;    // kg/m3

    // 音響タスクの型
    export enum class SynthesisType {
        Modal,      // インパクト音 (サイン波合成)
        Stochastic, // 雨・砂など (ショットノイズ/粒状合成)
        Flow,       // 風・気流 (ノイズフィルタリング)
        BandPassNoise, // 環境音の帯域制限ノイズ
        Friction,   // 摩擦 (テクスチャードノイズ)
        Droplet,    // 雨粒ごとの確率的な衝撃音
        BubbleCloud // 砕波で発生する泡群の共鳴
    };

    // 簡易的なベクトル構造体
    export struct Vector3 {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float Length() const { return std::sqrt(x*x + y*y + z*z); }
        Vector3 operator-(const Vector3& other) const { return {x-other.x, y-other.y, z-other.z}; }
        float Dot(const Vector3& other) const { return x*other.x + y*other.y + z*other.z; }
    };

    // A physical bubble sample from a fluid/acoustic bridge. Position and radius
    // are expressed in meters; visual foam sprite sizes must not be passed as radii.
    export struct AcousticBubbleSample {
        Vector3 position{};
        float radiusMeters = 0.001f;
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
        float eventRate = 0.0f; // Dropletイベント数/秒
        float panSpread = 0.0f; // Dropletイベントごとの左右幅
        bool waterImpact = false;
        float couplingStrength = 0.0f; // Bubble-cloud normalized coupling proxy
        float bubbleRadiusMeters = 0.0f; // Geometric-mean physical bubble radius; 0 selects the statistical model
    };

    // Small, fixed-capacity result for one physical model update.
    export struct AudioTaskBatch {
        static constexpr std::size_t Capacity = 8;
        std::array<AudioTask, Capacity> tasks{};
        std::size_t count = 0;

        bool append(const AudioTask& task) {
            if (count >= Capacity) return false;
            tasks[count++] = task;
            return true;
        }

        const AudioTask* begin() const { return tasks.data(); }
        const AudioTask* end() const { return tasks.data() + count; }
    };

    // Fixed-size handoff for the highest-priority tasks in one audio block.
    export struct AudioTaskBlock {
        static constexpr std::size_t Capacity = 32;
        std::array<AudioTask, Capacity> tasks{};
        std::size_t count = 0;

        std::span<const AudioTask> view() const { return {tasks.data(), count}; }
    };

    // 音響合成タスクのデバッグ用情報
    export struct AudioTaskDebug {
        std::uint32_t layerId;
        SynthesisType type;
        float freq;
        float amp;
        float qFactor = 1.0f;
        float duration;
        float pan = 0.0f;
        float doppler = 1.0f;
        float attenuation;
        std::uint32_t seed = 0;
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
        virtual AudioTaskBatch GenerateTasks() = 0;
    };
}
