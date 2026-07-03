module;
#include <utility>
#include "../Define/DllExportMacro.hpp"
#include <vector>
#include <string>
#include <cstdint>

export module Audio.FormantExtractor;

import Audio.Segment;

export namespace ArtifactCore {

/// フォルマント情報
struct FormantSet {
    float f1 = 0.0f; // 第一フォルマント (Hz) 開き具合
    float f2 = 0.0f; // 第二フォルマント (Hz) 舌位置
    float f3 = 0.0f; // 第三フォルマント (Hz) 唇丸め
    float intensity = 0.0f; // 全体的な強度
};

/// 音素ラベル
enum class PhonemeLabel : uint8_t {
    Silence = 0,
    A,  // /a/ あ
    I,  // /i/ い
    U,  // /u/ う
    E,  // /e/ え
    O,  // /o/ お
    M,  // /m/ /b/ /p/ 唇閉
    F,  // /f/ /v/ 下唇
    N,  // /n/ /t/ /d/ /s/
    L,  // /l/ /r/
    Other
};

/// 音素イベント（フレーム単位）
struct PhonemeEvent {
    int64_t frame = 0;
    PhonemeLabel label = PhonemeLabel::Silence;
    float intensity = 0.0f;
    float confidence = 0.0f;

    // 口形状インデックス（Switch Layer 連携用）
    int mouthShapeIndex() const {
        switch (label) {
            case PhonemeLabel::Silence: return 0; // 閉じる
            case PhonemeLabel::A:        return 1; // 大きく開く
            case PhonemeLabel::I:        return 2; // 横に引く
            case PhonemeLabel::U:        return 3; // 丸める
            case PhonemeLabel::E:        return 4; // やや開く
            case PhonemeLabel::O:        return 3; // 丸める (Uと同じ)
            case PhonemeLabel::M:        return 5; // 閉じる→開く
            case PhonemeLabel::F:        return 6; // 下唇
            case PhonemeLabel::N:        return 0; // 閉じる
            default:                     return 0;
        }
    }
};

/// フォルマント抽出 + 音素分類エンジン
class LIBRARY_DLL_API FormantExtractor {
public:
    FormantExtractor();
    ~FormantExtractor();

    /// 1回の AudioSegment からフォルマントを抽出
    FormantSet extractFormants(const AudioSegment& segment);

    /// フォルマント → 音素ラベル分類
    PhonemeLabel classifyPhoneme(const FormantSet& formants) const;

    /// 1フレーム分の解析
    PhonemeEvent analyzeFrame(const AudioSegment& segment, int64_t frame);

    /// 全フレームの連続解析
    std::vector<PhonemeEvent> analyzeTrack(
        const AudioSegment& segment,
        double frameRate,
        int64_t startFrame = 0);

    /// 母音判定の閾値調整
    void setVowelThresholds(float f1min, float f1max, float f2min, float f2max);
    
private:
    /// スペクトルピーク検出
    std::vector<float> findPeaks(const std::vector<float>& spectrum,
                                 float binSize,
                                 float minFreq = 200.0f,
                                 float maxFreq = 4000.0f);

    float f1Min_ = 200.0f;
    float f1Max_ = 1000.0f;
    float f2Min_ = 800.0f;
    float f2Max_ = 3000.0f;
};

} // namespace ArtifactCore
