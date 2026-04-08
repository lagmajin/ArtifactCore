module;
#include <utility>

export module Color.ACES;

import Color.ColorSpace;
import Color.GamutConversion;
import Color.TransferFunction;

export namespace ArtifactCore {

// ACES ワークフロー設定
enum class ACESWorkingSpace {
    ACEScg,     // AP1 リニア (コンポジション作業用、推奨)
    ACEScct,    // AP1 対数 (編集操作に適したダイナミックレンジ)
};

// ACES 出力変換プリセット
enum class ACESOutputTransform {
    SDR_sRGB,           // sRGB ディスプレイ (Rec.709, D65)
    SDR_Rec709,         // Rec.709 TV
    SDR_P3_D65,         // DCI-P3 D65 ディスプレイ
    HDR_Rec2020_PQ,     // Rec.2020 PQ (HDR10)
    HDR_P3_D65_HLG,     // P3 D65 HLG
};

// ACES 入力変換プリセット
enum class ACESInputTransform {
    sRGB_Linear,        // sRGB リニア入力
    sRGB_Encoded,       // sRGB エンコード入力 (ガンマ補正済み)
    Rec709_Encoded,     // Rec.709 エンコード入力
    Rec2020_Encoded,    // Rec.2020 エンコード入力
    P3_Encoded,         // P3 エンコード入力
    Linear_sRGB_Primaries, // リニア (sRGB プライマリ)
};

// ACES カラーマネージャー
// IDT (Input Device Transform) → ACES → RRT+ODT の簡易実装
// ※ RRT+ODT の完全実装は非常に複雑。ここではカラースペース変換 + 伝達関数変換の簡易版。
class ACESColorManager {
public:
    ACESColorManager() = default;

    // 作業色域の設定
    void setWorkingSpace(ACESWorkingSpace space) {
        workingSpace_ = space;
    }

    ACESWorkingSpace workingSpace() const { return workingSpace_; }

    // 入力変換: 入力色空間 → ACES 作業色域
    // 3 チャンネルの RGB 値を変換
    static void applyInputTransform(
        float& r, float& g, float& b,
        ACESInputTransform inputTransform,
        ACESWorkingSpace workingSpace = ACESWorkingSpace::ACEScg)
    {
        // Step 1: 入力色空間からリニアに変換
        applyEOTF(r, g, b, inputTransform);

        // Step 2: プライマリを XYZ 経由で作業色域に変換
        Gamut inputGamut = inputGamutFromTransform(inputTransform);
        Gamut workingGamut = (workingSpace == ACESWorkingSpace::ACEScg)
            ? Gamut::ACES_AP1 : Gamut::ACES_AP1;

        if (inputGamut != workingGamut) {
            float outR = r;
            float outG = g;
            float outB = b;
            ColorGamutConversion::convert(r, g, b, inputGamut, workingGamut,
                                          outR, outG, outB);
            r = outR;
            g = outG;
            b = outB;
        }
    }

    // 出力変換: ACES 作業色域 → 出力色空間
    // 簡易 RRT+ODT 近似: ガンマ補正 + ガマットマッピング
    static void applyOutputTransform(
        float& r, float& g, float& b,
        ACESOutputTransform outputTransform,
        ACESWorkingSpace workingSpace = ACESWorkingSpace::ACEScg)
    {
        Gamut workingGamut = (workingSpace == ACESWorkingSpace::ACEScg)
            ? Gamut::ACES_AP1 : Gamut::ACES_AP1;

        // Step 1: 作業色域から出力プライマリに変換
        Gamut outputGamut = outputGamutFromTransform(outputTransform);
        if (workingGamut != outputGamut) {
            float outR = r;
            float outG = g;
            float outB = b;
            ColorGamutConversion::convert(r, g, b, workingGamut, outputGamut,
                                          outR, outG, outB);
            r = outR;
            g = outG;
            b = outB;
        }

        // Step 2: 簡易 RRT (Reference Rendering Transform)
        // ACES のダイナミックレンジを適切に圧縮
        applySimpleRRT(r, g, b);

        // Step 3: OETF (出力伝達関数) を適用
        applyOETF(r, g, b, outputTransform);
    }

    // 3x3 行列として変換を取得 (GPU シェーダー用)
    static Matrix3x3 getInputConversionMatrix(
        ACESInputTransform inputTransform,
        ACESWorkingSpace workingSpace = ACESWorkingSpace::ACEScg)
    {
        Gamut inputGamut = inputGamutFromTransform(inputTransform);
        Gamut workingGamut = Gamut::ACES_AP1;

        // リニア化ガンマを含む変換行列
        return ColorGamutConversion::getConversionMatrix(inputGamut, workingGamut);
    }

    static Matrix3x3 getOutputConversionMatrix(
        ACESOutputTransform outputTransform,
        ACESWorkingSpace workingSpace = ACESWorkingSpace::ACEScg)
    {
        Gamut workingGamut = Gamut::ACES_AP1;
        Gamut outputGamut = outputGamutFromTransform(outputTransform);
        return ColorGamutConversion::getConversionMatrix(workingGamut, outputGamut);
    }

    // ガマット境界チェック (ACES 作業色域外の値を検出)
    static bool isInGamut(float r, float g, float b) {
        // ACEScg (AP1 リニア) の有効範囲: [0, 1+] (ACES はシーン参照なので値は超え得る)
        return r >= 0.0f && g >= 0.0f && b >= 0.0f;
    }

    // ソフトクリップ (ガマット境界付近の色をスムーズに圧縮)
    static void softClip(float& r, float& g, float& b, float limit = 1.0f) {
        auto softLimit = [limit](float v) -> float {
            if (v <= limit) return v;
            float excess = v - limit;
            return limit + excess / (1.0f + excess);
        };
        r = softLimit(r);
        g = softLimit(g);
        b = softLimit(b);
    }

    // ACES 出力変換の説明文字列
    static const char* transformName(ACESOutputTransform t) {
        switch (t) {
            case ACESOutputTransform::SDR_sRGB:         return "SDR sRGB (Rec.709)";
            case ACESOutputTransform::SDR_Rec709:       return "SDR Rec.709";
            case ACESOutputTransform::SDR_P3_D65:       return "SDR P3 D65";
            case ACESOutputTransform::HDR_Rec2020_PQ:   return "HDR Rec.2020 PQ";
            case ACESOutputTransform::HDR_P3_D65_HLG:   return "HDR P3 D65 HLG";
            default: return "Unknown";
        }
    }

private:
    ACESWorkingSpace workingSpace_ = ACESWorkingSpace::ACEScg;

    // EOTF 適用 (エンコード → リニア)
    static void applyEOTF(float& r, float& g, float& b, ACESInputTransform transform) {
        TransferFunction tf = TransferFunction::Linear;
        switch (transform) {
            case ACESInputTransform::sRGB_Encoded:       tf = TransferFunction::sRGB; break;
            case ACESInputTransform::Rec709_Encoded:     tf = TransferFunction::Rec709; break;
            case ACESInputTransform::Rec2020_Encoded:    tf = TransferFunction::Gamma24; break;
            case ACESInputTransform::P3_Encoded:         tf = TransferFunction::sRGB; break; // P3 は sRGB 同等の伝達関数
            case ACESInputTransform::sRGB_Linear:
            case ACESInputTransform::Linear_sRGB_Primaries:
            default: return; // 既にリニア
        }
        r = ColorTransferFunction::decode(r, tf);
        g = ColorTransferFunction::decode(g, tf);
        b = ColorTransferFunction::decode(b, tf);
    }

    // OETF 適用 (リニア → エンコード)
    static void applyOETF(float& r, float& g, float& b, ACESOutputTransform transform) {
        TransferFunction tf;
        switch (transform) {
            case ACESOutputTransform::SDR_sRGB:         tf = TransferFunction::sRGB; break;
            case ACESOutputTransform::SDR_Rec709:       tf = TransferFunction::Rec709; break;
            case ACESOutputTransform::SDR_P3_D65:       tf = TransferFunction::sRGB; break;
            case ACESOutputTransform::HDR_Rec2020_PQ:   tf = TransferFunction::Rec2084_PQ; break;
            case ACESOutputTransform::HDR_P3_D65_HLG:   tf = TransferFunction::HLG; break;
            default: return;
        }
        r = ColorTransferFunction::encode(r, tf);
        g = ColorTransferFunction::encode(g, tf);
        b = ColorTransferFunction::encode(b, tf);
    }

    // 簡易 RRT: ACES のシーン参照値を表示可能な範囲に圧縮
    // 完全な RRT+ODT は Academy の CTL コードに依存するが、
    // ここではランプ関数ベースの簡易近似を使用
    static void applySimpleRRT(float& r, float& g, float& b) {
        // ACES の約 0.18 を中間輝度として対数圧縮
        constexpr float midGray = 0.18f;
        constexpr float slope = 2.51f;
        constexpr float offset = 0.03f;
        constexpr float shoulder = 2.43f;
        constexpr float toe = 0.59f;
        constexpr float knee = 0.14f;

        // "ACES Filmic" 近似 (RRT+ODT の簡易版)
        // (x * (slope * x + offset)) / (x * (shoulder * x + toe) + knee)
        auto filmic = [](float x) -> float {
            if (x <= 0.0f) return 0.0f;
            float num = x * (slope * x + offset);
            float den = x * (shoulder * x + toe) + knee;
            return num / den;
        };

        r = filmic(r);
        g = filmic(g);
        b = filmic(b);
    }

    // 入力変換からガマットを取得
    static Gamut inputGamutFromTransform(ACESInputTransform transform) {
        switch (transform) {
            case ACESInputTransform::sRGB_Linear:
            case ACESInputTransform::sRGB_Encoded:
            case ACESInputTransform::Linear_sRGB_Primaries:
                return Gamut::sRGB;
            case ACESInputTransform::Rec709_Encoded:
                return Gamut::Rec709;
            case ACESInputTransform::Rec2020_Encoded:
                return Gamut::Rec2020;
            case ACESInputTransform::P3_Encoded:
                return Gamut::DCI_P3;
            default:
                return Gamut::sRGB;
        }
    }

    // 出力変換からガマットを取得
    static Gamut outputGamutFromTransform(ACESOutputTransform transform) {
        switch (transform) {
            case ACESOutputTransform::SDR_sRGB:
            case ACESOutputTransform::SDR_Rec709:
                return Gamut::Rec709;
            case ACESOutputTransform::SDR_P3_D65:
                return Gamut::DCI_P3;
            case ACESOutputTransform::HDR_Rec2020_PQ:
            case ACESOutputTransform::HDR_P3_D65_HLG:
                return Gamut::Rec2020;
            default:
                return Gamut::Rec709;
        }
    }
};

} // namespace ArtifactCore
