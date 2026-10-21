module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>
#include <QJsonObject>
#include <QtGlobal>
#include "../Define/DllExportMacro.hpp"

export module Audio.Effect.PlateReverb;
import Core.ArtifactString;

import Audio.Effect;
import Audio.Segment;

export namespace ArtifactCore {

/**
 * @brief Plate Reverb エフェクト(Core 側コア)
 * Dattorro 式プレートの小型コア。入力ディフュージョン(4 直列オールパス)、
 * クロスカップルド 2 タンク、タンク内オールパス、1 ポールダンピング、
 * LFO モジュレーションで構成する。
 * プレート実機どおりモノラル入力→ステレオ出力とし、ステレオ入力は
 * タンク前でモノラル化する。3ch 以上は先頭 2ch だけ処理し、残りは
 * Dry のまま残す(AudioReverb / AudioChorus と同じ扱い)。
 * 既存 AudioReverb(簡易 Schroeder)とは別物。effectType は "plate_reverb"。
 */
class LIBRARY_DLL_API AudioPlateReverb : public AudioEffect {
public:
    AudioPlateReverb();
    virtual ~AudioPlateReverb() = default;

    String getName() const override { return "Plate Reverb"; }
    String effectType() const override { return "plate_reverb"; }
    void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) override;

    // Pre-delay は Wet 経路のみで出力時刻は動かさないため遅延なし。
    qint64 latencySamples() const override { return 0; }
    qint64 tailSamples() const override;

    void setPreDelayMs(float v) {
        preDelayMs_ = std::isfinite(v) ? std::clamp(v, 0.0f, 100.0f) : 10.0f;
    }
    void setDecay(float v) {
        decay_ = std::isfinite(v) ? std::clamp(v, 0.0f, 0.99f) : 0.75f;
    }
    void setDamping(float v) {
        damping_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.4f;
    }
    void setDiffusion(float v) {
        diffusion_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.7f;
    }
    void setSize(float v) {
        size_ = std::isfinite(v) ? std::clamp(v, 0.5f, 2.0f) : 1.0f;
    }
    void setModDepth(float v) {
        modDepth_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.3f;
    }
    void setModRate(float v) {
        modRate_ = std::isfinite(v) ? std::clamp(v, 0.1f, 5.0f) : 0.8f;
    }
    void setWidth(float v) {
        width_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.8f;
    }
    void setMix(float v) {
        mix_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.3f;
    }

    float getPreDelayMs() const { return preDelayMs_; }
    float getDecay() const { return decay_; }
    float getDamping() const { return damping_; }
    float getDiffusion() const { return diffusion_; }
    float getSize() const { return size_; }
    float getModDepth() const { return modDepth_; }
    float getModRate() const { return modRate_; }
    float getWidth() const { return width_; }
    float getMix() const { return mix_; }

    std::vector<EffectParameter> getParameters() const override;
    void setParameterValue(const String& id, float value) override;
    float getParameterValue(const String& id) const override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;

private:
    // Dattorro 基準レートと基準遅延(単位 sample)。実遅延は
    // ref * size_ * (sampleRate / kRefSampleRate) で求める。
    static constexpr float kRefSampleRate = 29761.0f;
    static constexpr float kInDiffRef[4] = {142.0f, 107.0f, 379.0f, 277.0f};
    static constexpr float kTankRef[2] = {672.0f, 908.0f};
    static constexpr float kTankApRef[2] = {908.0f, 672.0f};

    // sampleRate / size_ に応じた遅延長の再計算と確保。process 内から
    // 呼ばれるが、再確保はレート・サイズ変化時だけの bounded な確保で、
    // steady-state の毎ブロック確保ではない(HOT_PATH_RULES 例外の記録)。
    void ensureCapacity(int sampleRate);
    void processPlateSample(float monoIn, float& wetL, float& wetR);

    float preDelayMs_ = 10.0f;
    float decay_ = 0.75f;
    float damping_ = 0.4f;
    float diffusion_ = 0.7f;
    float size_ = 1.0f;
    float modDepth_ = 0.3f;
    float modRate_ = 0.8f;
    float width_ = 0.8f;
    float mix_ = 0.3f;

    // Instance-safe buffers (スレッド共有なし。AudioReverb と同じ方針)。
    std::vector<float> preBuf_;
    int prePos_ = 0;
    std::vector<float> diffBuf_[4];
    int diffPos_[4] = {0, 0, 0, 0};
    std::vector<float> tankBuf_[2];
    int tankPos_[2] = {0, 0};
    std::vector<float> tankApBuf_[2];
    int tankApPos_[2] = {0, 0};

    float lfoPhase_[2] = {0.0f, 0.0f};
    float dampState_[2] = {0.0f, 0.0f};
    float tankAccum_[2] = {0.0f, 0.0f};

    int builtSampleRate_ = -1;
    float builtSize_ = -1.0f;
};

} // namespace ArtifactCore
