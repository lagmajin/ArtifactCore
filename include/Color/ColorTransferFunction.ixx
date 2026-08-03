module;
#include <utility>
#include <cmath>
#include <algorithm>

export module Color.TransferFunction;

export namespace ArtifactCore
{

 /// トランスファ関数 (EOTF/OETF) の列挙。
 /// DaVinci Resolve の Color Science メニューに対応。
 export enum class TransferFunction {
  Linear,              // 線形 (シーン参照)
  sRGB,                // IEC 61966-2-1 (ディスプレイ)
  Gamma22,             // 単純ガンマ 2.2
  Gamma24,             // 単純ガンマ 2.4
  Gamma26,             // 単純ガンマ 2.6 (DCI)
  Rec709,              // ITU-R BT.709 OETF
  Rec2020_10,          // ITU-R BT.2020 (10bit) OETF
  Rec2084_PQ,          // SMPTE ST 2084 (PQ) HDR
  HLG,                 // ARIB STD-B67 (HLG)
  ACEScc,              // ACEScc 対数エンコーディング
  ACEScct,             // ACEScct 対数エンコーディング (toe 付き)
  ACESlog,             // ACESlog (ACES2065-1 用)
  DaVinciIntermediate, // DaVinci Intermediate 対数エンコーディング
  Cineon,              // Cineon/DPX 10bit Log
  SonySLog3,           // Sony S-Log3
  CanonLog2,           // Canon Log 2
  CanonLog3            // Canon Log 3
 };

 /// トランスファ関数の変換ユーティリティ。
 /// 全関数は [0,1] 正規化入力を受け取り、[0,1] 正規化出力を返す。
 class ColorTransferFunction {
 public:

  // === sRGB (IEC 61966-2-1) ===

  /// 線形 → sRGB (EOTF の逆 = OETF)
  static float linearToSRGB(float linear) {
   linear = std::max(linear, 0.0f);
   if (linear <= 0.0031308f)
    return 12.92f * linear;
   return 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
  }

  /// sRGB → 線形 (EOTF)
  static float srgbToLinear(float srgb) {
   srgb = std::max(srgb, 0.0f);
   if (srgb <= 0.04045f)
    return srgb / 12.92f;
   return std::pow((srgb + 0.055f) / 1.055f, 2.4f);
  }

  // === 単純ガンマ ===

  static float linearToGamma(float linear, float gamma) {
   return std::pow(std::max(linear, 0.0f), 1.0f / gamma);
  }

  static float gammaToLinear(float encoded, float gamma) {
   return std::pow(std::max(encoded, 0.0f), gamma);
  }

  // === Rec.709 OETF ===

  static float linearToRec709(float linear) {
   linear = std::max(linear, 0.0f);
   if (linear < 0.018f)
    return 4.5f * linear;
   return 1.099f * std::pow(linear, 0.45f) - 0.099f;
  }

  static float rec709ToLinear(float encoded) {
   encoded = std::max(encoded, 0.0f);
   if (encoded < 0.081f)
    return encoded / 4.5f;
   return std::pow((encoded + 0.099f) / 1.099f, 1.0f / 0.45f);
  }

  // === Rec.2020 10-bit OETF ===
  // ITU-R BT.2020 uses the same power-law shape as BT.709 with the
  // BT.2020 breakpoint and coefficients.
  static float linearToRec2020(float linear) {
   constexpr float alpha = 1.09929682680944f;
   constexpr float beta = 0.018053968510807f;
   linear = std::max(linear, 0.0f);
   if (linear < beta)
    return 4.5f * linear;
   return alpha * std::pow(linear, 0.45f) - (alpha - 1.0f);
  }

  static float rec2020ToLinear(float encoded) {
   constexpr float alpha = 1.09929682680944f;
   constexpr float beta = 0.081242858298635f;
   encoded = std::max(encoded, 0.0f);
   if (encoded < beta)
    return encoded / 4.5f;
   return std::pow((encoded + alpha - 1.0f) / alpha, 1.0f / 0.45f);
  }

  // === Rec.2084 PQ ===

  static constexpr float kPQ_m1 = 0.1593017578125f;    // 2610 / 16384
  static constexpr float kPQ_m2 = 78.84375f;            // 2523 / 32 * 128
  static constexpr float kPQ_c1 = 0.8359375f;           // 3424 / 4096
  static constexpr float kPQ_c2 = 18.8515625f;          // 2413 / 128
  static constexpr float kPQ_c3 = 18.6875f;             // 2392 / 128

  /// 線形 (0=0, 1=10000nits 正規化) → PQ OETF
  static float linearToPQ(float linear) {
   if (linear <= 0.0f) return 0.0f;
   float ym1 = std::pow(linear, kPQ_m1);
   float num = kPQ_c1 + kPQ_c2 * ym1;
   float den = 1.0f + kPQ_c3 * ym1;
   return std::pow(num / den, kPQ_m2);
  }

  /// PQ → 線形 (0=0, 1=10000nits 正規化)
  static float pqToLinear(float pq) {
   if (pq <= 0.0f) return 0.0f;
   float ep = std::pow(pq, 1.0f / kPQ_m2);
   float num = std::max(ep - kPQ_c1, 0.0f);
   float den = kPQ_c2 - kPQ_c3 * ep;
   if (den <= 0.0f) return 0.0f;
   return std::pow(num / den, 1.0f / kPQ_m1);
  }

  // === HLG (ARIB STD-B67) ===

  static constexpr float kHLG_a = 0.17883277f;
  static constexpr float kHLG_b = 0.28466892f;  // 1 - 4*a
  static constexpr float kHLG_c = 0.55991073f;  // 0.5 - a * ln(4a)

  static float linearToHLG(float linear) {
   linear = std::max(linear, 0.0f);
   if (linear <= 1.0f / 12.0f)
    return std::sqrt(3.0f * linear);
   return kHLG_a * std::log(12.0f * linear - kHLG_b) + kHLG_c;
  }

  static float hlgToLinear(float hlg) {
   hlg = std::max(hlg, 0.0f);
   if (hlg <= 0.5f)
    return (hlg * hlg) / 3.0f;
   return (std::exp((hlg - kHLG_c) / kHLG_a) + kHLG_b) / 12.0f;
  }

  // === ACEScc ===
  // ACEScc = (9.72 - log2(ACES_linear * 0.5 + 0.000030517578125)) / 17.52
  // Ref: SMPTE ST 2065-4

  static float linearToACEScc(float linear) {
   if (linear <= 0.0f)
    return -0.3584474886f;  // log2(0.000030517578125) related
   return (9.72f - std::log2(linear * 0.5f + 0.000030517578125f)) / 17.52f;
  }

  static float acesccToLinear(float cc) {
   return (std::pow(2.0f, 9.72f - cc * 17.52f) - 0.000030517578125f) * 2.0f;
  }

  // === ACEScct ===
  // ACEScct は toe 領域を持つ ACEScc の改良版。
  // Ref: TB-2018-001

  static float linearToACEScct(float linear) {
   if (linear <= 0.0078125f)
    return 10.5402377416545f * linear + 0.0729055341958355f;
   return (std::log2(linear) + 9.72f) / 17.52f;
  }

  static float acescctToLinear(float cct) {
   if (cct <= 0.155251141552511f)
    return (cct - 0.0729055341958355f) / 10.5402377416545f;
   return std::pow(2.0f, cct * 17.52f - 9.72f);
  }

  // === DaVinci Intermediate ===
  // Ref: Blackmagic Design, "DaVinci Wide Gamut / DaVinci Intermediate"
  // linear → DI: (log2(linear) + 12.473931188) / 12.900429241

  static float linearToDaVinciIntermediate(float linear) {
   if (linear <= 0.0f)
    return 0.0f;
   return (std::log2(linear) + 12.473931188f) / 12.900429241f;
  }

  static float daVinciIntermediateToLinear(float di) {
   return std::pow(2.0f, di * 12.900429241f - 12.473931188f);
  }

  // === Sony S-Log3 ===

  static float linearToSLog3(float linear) {
   linear = std::max(linear, 0.0f);
   if (linear >= 0.01125000f)
    return (420.0f + std::log10((linear + 0.01f) / (0.18f + 0.01f)) * 261.5f) / 1023.0f;
   return (linear * (171.2102946929f - 95.0f) / 0.01125000f + 95.0f) / 1023.0f;
  }

  static float sLog3ToLinear(float slog3) {
   slog3 = slog3 * 1023.0f;
   if (slog3 >= 171.2102946929f)
    return std::pow(10.0f, (slog3 - 420.0f) / 261.5f) * (0.18f + 0.01f) - 0.01f;
   return (slog3 - 95.0f) * 0.01125000f / (171.2102946929f - 95.0f);
  }

  // === Kodak Cineon ===
  // Normalized 10-bit printing-density encoding. The default black offset
  // corresponds to Cineon code value 95.
  static constexpr float kCineonBlackOffset = 0.0107977516232771f;

  static float linearToCineon(float linear) {
   const float value = std::max(linear, 0.0f);
   return (685.0f + 300.0f * std::log10(
       value * (1.0f - kCineonBlackOffset) + kCineonBlackOffset)) / 1023.0f;
  }

  static float cineonToLinear(float encoded) {
   return (std::pow(10.0f, (1023.0f * encoded - 685.0f) / 300.0f) -
           kCineonBlackOffset) / (1.0f - kCineonBlackOffset);
  }

  // === Canon Log 2 / Canon Log 3 ===
  // Curves are expressed in normalized IRE units, as published by Canon.
  static float linearToCanonLog2(float linear) {
   constexpr float toe = 0.035388128f;
   constexpr float slope = 0.281863093f;
   constexpr float scale = 87.09937546f;
   const float toeLinear = -(std::pow(10.0f, toe / slope) - 1.0f) / scale;
   if (linear < toeLinear)
    return -(slope * std::log10(-linear * scale + 1.0f) - toe);
   return slope * std::log10(linear * scale + 1.0f) + toe;
  }

  static float canonLog2ToLinear(float encoded) {
   constexpr float toe = 0.035388128f;
   constexpr float slope = 0.281863093f;
   constexpr float scale = 87.09937546f;
   if (encoded < toe)
    return -(std::pow(10.0f, (toe - encoded) / slope) - 1.0f) / scale;
   return (std::pow(10.0f, (encoded - toe) / slope) - 1.0f) / scale;
  }

  static float linearToCanonLog3(float linear) {
   constexpr float low = 0.04076162f;
   constexpr float high = 0.105357102f;
   constexpr float toe = 0.069886632f;
   constexpr float slope = 0.42889912f;
   constexpr float scale = 14.98325f;
   const float lowLinear = -(std::pow(10.0f, (toe - low) / slope) - 1.0f) / scale;
   const float highLinear = (high - 0.073059361f) / 2.3069815f;
   if (linear < lowLinear)
    return -(slope * std::log10(-linear * scale + 1.0f) - toe);
   if (linear <= highLinear)
    return 2.3069815f * linear + 0.073059361f;
   return slope * std::log10(linear * scale + 1.0f) + toe;
  }

  static float canonLog3ToLinear(float encoded) {
   constexpr float low = 0.04076162f;
   constexpr float high = 0.105357102f;
   constexpr float toe = 0.069886632f;
   constexpr float slope = 0.42889912f;
   constexpr float scale = 14.98325f;
   if (encoded < low)
    return -(std::pow(10.0f, (toe - encoded) / slope) - 1.0f) / scale;
   if (encoded <= high)
    return (encoded - 0.073059361f) / 2.3069815f;
   return (std::pow(10.0f, (encoded - toe) / slope) - 1.0f) / scale;
  }

  // === 汎用: 任意の TransferFunction で線形↔エンコード変換 ===

  static float encode(float linear, TransferFunction tf) {
   switch (tf) {
    case TransferFunction::sRGB:                 return linearToSRGB(linear);
    case TransferFunction::Gamma22:              return linearToGamma(linear, 2.2f);
    case TransferFunction::Gamma24:              return linearToGamma(linear, 2.4f);
    case TransferFunction::Gamma26:              return linearToGamma(linear, 2.6f);
    case TransferFunction::Rec709:               return linearToRec709(linear);
    case TransferFunction::Rec2020_10:            return linearToRec2020(linear);
    case TransferFunction::Rec2084_PQ:           return linearToPQ(linear);
    case TransferFunction::HLG:                  return linearToHLG(linear);
    case TransferFunction::ACEScc:               return linearToACEScc(linear);
    case TransferFunction::ACEScct:              return linearToACEScct(linear);
    case TransferFunction::DaVinciIntermediate:  return linearToDaVinciIntermediate(linear);
    case TransferFunction::SonySLog3:            return linearToSLog3(linear);
    case TransferFunction::Cineon:               return linearToCineon(linear);
    case TransferFunction::CanonLog2:            return linearToCanonLog2(linear);
    case TransferFunction::CanonLog3:            return linearToCanonLog3(linear);
    case TransferFunction::Linear:
    default:                                     return linear;
   }
  }

  static float decode(float encoded, TransferFunction tf) {
   switch (tf) {
    case TransferFunction::sRGB:                 return srgbToLinear(encoded);
    case TransferFunction::Gamma22:              return gammaToLinear(encoded, 2.2f);
    case TransferFunction::Gamma24:              return gammaToLinear(encoded, 2.4f);
    case TransferFunction::Gamma26:              return gammaToLinear(encoded, 2.6f);
    case TransferFunction::Rec709:               return rec709ToLinear(encoded);
    case TransferFunction::Rec2020_10:            return rec2020ToLinear(encoded);
    case TransferFunction::Rec2084_PQ:           return pqToLinear(encoded);
    case TransferFunction::HLG:                  return hlgToLinear(encoded);
    case TransferFunction::ACEScc:               return acesccToLinear(encoded);
    case TransferFunction::ACEScct:              return acescctToLinear(encoded);
    case TransferFunction::DaVinciIntermediate:  return daVinciIntermediateToLinear(encoded);
    case TransferFunction::SonySLog3:            return sLog3ToLinear(encoded);
    case TransferFunction::Cineon:               return cineonToLinear(encoded);
    case TransferFunction::CanonLog2:            return canonLog2ToLinear(encoded);
    case TransferFunction::CanonLog3:            return canonLog3ToLinear(encoded);
    case TransferFunction::Linear:
    default:                                     return encoded;
   }
  }
 };

}
