module;
#include <utility>
#include <cmath>
#include <algorithm>

export module Color.GamutConversion;

export namespace ArtifactCore
{

 /// ガムット (色域) の列挙。
 export enum class Gamut {
  sRGB,       // IEC 61966-2-1 (Rec.709 primaries, D65)
  Rec709,     // ITU-R BT.709 (sRGB と同等プライマリ)
  Rec2020,    // ITU-R BT.2020 (ワイド色域)
  DCI_P3,     // DCI-P3 (シネマ)
  DisplayP3,  // Display P3 (Apple, D65)
  ACES_AP0,   // ACES2065-1 (AP0, アーカイブ)
  ACES_AP1,   // ACEScg (AP1, コンポジット作業色域)
  AdobeRGB,   // Adobe RGB (1998)
  XYZ_D65,    // CIE XYZ (D65)
  XYZ_D60,    // CIE XYZ (D60, ACES 用)
  DaVinciWideGamut // Blackmagic DaVinci Wide Gamut
 };

 /// 3x3 行列。行優先。
 export struct Matrix3x3 {
  float m[3][3];

  float* operator[](int row) { return m[row]; }
  const float* operator[](int row) const { return m[row]; }

  /// 行列 × ベクトル
  float operator()(int row, float r, float g, float b) const {
   return m[row][0] * r + m[row][1] * g + m[row][2] * b;
  }
 };

 /// RGB ベクトルの行列変換。
 export inline void multiply(const Matrix3x3& mat, float r, float g, float b,
                              float& outR, float& outG, float& outB) {
  outR = mat[0][0]*r + mat[0][1]*g + mat[0][2]*b;
  outG = mat[1][0]*r + mat[1][1]*g + mat[1][2]*b;
  outB = mat[2][0]*r + mat[2][1]*g + mat[2][2]*b;
 }

 /// 行列の乗算 (A × B)。
 export inline Matrix3x3 multiply(const Matrix3x3& a, const Matrix3x3& b) {
  Matrix3x3 result{};
  for (int i = 0; i < 3; ++i)
   for (int j = 0; j < 3; ++j)
    result[i][j] = a[i][0]*b[0][j] + a[i][1]*b[1][j] + a[i][2]*b[2][j];
  return result;
 }

 /// 単位行列。
 export inline Matrix3x3 identity() {
  return {{{1,0,0},{0,1,0},{0,0,1}}};
 }

 /// ガムット変換マネージャ。
 /// DaVinci Resolve の Color Management に相当。
 class ColorGamutConversion {
 public:

  // === プリセット行列 ===

  // Rec.709 (sRGB) プライマリ (D65)
  // x: 0.64, 0.30, 0.15  y: 0.33, 0.60, 0.06
  static Matrix3x3 rec709ToXYZ() {
   return {{{0.4123908f, 0.3575843f, 0.1804808f},
            {0.2126390f, 0.7151687f, 0.0721923f},
            {0.0193308f, 0.1191950f, 0.9505321f}}};
  }
  static Matrix3x3 xyzToRec709() {
   return {{{ 3.2409699f,-1.5373832f,-0.4986108f},
            {-0.9692436f, 1.8759675f, 0.0415551f},
            { 0.0556301f,-0.2039770f, 1.0569715f}}};
  }

  // Rec.2020 プライマリ (D65)
  // x: 0.708, 0.170, 0.131  y: 0.292, 0.797, 0.046
  static Matrix3x3 rec2020ToXYZ() {
   return {{{0.6369580f, 0.1446169f, 0.1688810f},
            {0.2627002f, 0.6779981f, 0.0593017f},
            {0.0000000f, 0.0280727f, 1.0609851f}}};
  }
  static Matrix3x3 xyzToRec2020() {
   return {{{ 1.7166512f,-0.3556708f,-0.2533663f},
            {-0.6666844f, 1.6164812f, 0.0157685f},
            { 0.0176399f,-0.0427706f, 0.9421031f}}};
  }

  // DCI-P3 プライマリ (D65 ガマット変換用)
  static Matrix3x3 dciP3ToXYZ() {
   return {{{0.4451698f, 0.2771344f, 0.1722827f},
            {0.2094917f, 0.7215953f, 0.0689131f},
            {0.0000000f, 0.0470606f, 0.9073554f}}};
  }
  static Matrix3x3 xyzToDCIP3() {
   return {{{ 2.7253940f,-1.0180030f,-0.4401632f},
            {-0.7951680f, 1.6897321f, 0.0226472f},
            { 0.0412419f,-0.0876390f, 1.1009294f}}};
  }

  // Display P3 (Apple, D65)
  static Matrix3x3 displayP3ToXYZ() {
   return {{{0.4865709f, 0.2656677f, 0.1982173f},
            {0.2289746f, 0.6917385f, 0.0792869f},
            {0.0000000f, 0.0451134f, 1.0439444f}}};
  }
  static Matrix3x3 xyzToDisplayP3() {
   return {{{ 2.4934969f,-0.9313836f,-0.4027108f},
            {-0.8294890f, 1.7626641f, 0.0236247f},
            { 0.0358458f,-0.0761724f, 0.9568845f}}};
  }

  // ACES AP0 → XYZ (D60)
  // AP0 プライマリ: x: 0.7347, 0.0, 0.0001  y: 0.2653, 1.0, -0.0770
  static Matrix3x3 acesAP0ToXYZ() {
   return {{{0.9525523959f, 0.0000000000f, 0.0000936786f},
            {0.3439664498f, 0.7281660966f,-0.0721325464f},
            {0.0000000000f, 0.0000000000f, 1.0088251844f}}};
  }
  static Matrix3x3 xyzToAcesAP0() {
   return {{{ 1.0498110175f, 0.0000000000f,-0.0000974845f},
            {-0.4959030231f, 1.3733130458f, 0.0982400361f},
            { 0.0000000000f, 0.0000000000f, 0.9912520182f}}};
  }

  // ACES AP1 (ACEScg) → XYZ (D60)
  static Matrix3x3 acesAP1ToXYZ() {
   return {{{0.6624541811f, 0.1340042065f, 0.1561876870f},
            {0.2722287168f, 0.6740817658f, 0.0536895174f},
            {-0.0055746495f, 0.0040607335f, 1.0103391003f}}};
  }
  static Matrix3x3 xyzToAcesAP1() {
   return {{{ 1.6410233797f,-0.3248032942f,-0.2364246952f},
            {-0.6636628587f, 1.6153315917f, 0.0167563477f},
            { 0.0117218943f,-0.0082844420f, 0.9883948585f}}};
  }

  // AdobeRGB (1998) → XYZ (D65)
  static Matrix3x3 adobeRGBToXYZ() {
   return {{{0.6097559f, 0.2052401f, 0.1492240f},
            {0.3111242f, 0.6256560f, 0.0632197f},
            {0.0194816f, 0.0608902f, 0.7448387f}}};
  }
  static Matrix3x3 xyzToAdobeRGB() {
   return {{{ 1.9624274f,-0.6105343f,-0.3413404f},
            {-0.9787684f, 1.9161415f, 0.0334540f},
            { 0.0286869f,-0.1406752f, 1.3487655f}}};
  }

  // DaVinci Wide Gamut → XYZ (D65)
  // DWG プライマリ: x: 0.780308, 0.121595, 0.095612  y: 0.304253, 1.493994, -0.084589
  // Ref: Blackmagic Design
  static Matrix3x3 dawgToXYZ() {
   return {{{0.7007449f, 0.1487773f, 0.1010455f},
            {0.2741010f, 0.8736398f,-0.1477407f},
            {-0.0989175f, 0.1576015f, 0.9016143f}}};
  }
  static Matrix3x3 xyzToDawg() {
   return {{{ 1.5435783f,-0.2488530f,-0.1820373f},
            {-0.4826754f, 1.2609685f, 0.1746234f},
            { 0.1666435f,-0.2304632f, 1.1159845f}}};
  }

  // === D60 ↔ D65 ホワイトポイント変換 (Bradford) ===

  static Matrix3x3 d60ToD65() {
   return {{{ 0.9872240f,-0.0061133f, 0.0159533f},
            {-0.0075984f, 1.0018600f, 0.0053300f},
            { 0.0030727f,-0.0050960f, 1.0816800f}}};
  }
  static Matrix3x3 d65ToD60() {
   return {{{ 1.0130300f, 0.0061053f,-0.0149710f},
            { 0.0076873f, 0.9981640f,-0.0050320f},
            {-0.0028408f, 0.0046848f, 0.9245070f}}};
  }

  // === 変換行列取得 ===

  /// 指定した 2 つのガムット間の変換行列を返す。
  /// 経由: source → XYZ → Bradford WP 変換 → target
  static Matrix3x3 getConversionMatrix(Gamut source, Gamut target) {
   if (source == target) return identity();

   // 各ガムットの XYZ 変換関数を取得
   auto srcToXYZ = gamutToXYZ(source);
   auto tgtFromXYZ = xyzToGamut(target);

   // D60/D65 ホワイトポイント変換が必要か判定
   bool srcIsD60 = (source == Gamut::ACES_AP0 || source == Gamut::ACES_AP1);
   bool tgtIsD60 = (target == Gamut::ACES_AP0 || target == Gamut::ACES_AP1);

   if (srcIsD60 && !tgtIsD60) {
    // D60 → D65
    auto d60d65 = d60ToD65();
    auto xyzToD65 = multiply(d60d65, srcToXYZ);
    return multiply(tgtFromXYZ, xyzToD65);
   } else if (!srcIsD60 && tgtIsD60) {
    // D65 → D60
    auto d65d60 = d65ToD60();
    auto xyzToD60 = multiply(d65d60, srcToXYZ);
    return multiply(tgtFromXYZ, xyzToD60);
   }

   return multiply(tgtFromXYZ, srcToXYZ);
  }

  /// RGB ベクトルを別のガムットに変換。
  static void convert(float r, float g, float b,
                       Gamut source, Gamut target,
                       float& outR, float& outG, float& outB) {
   auto mat = getConversionMatrix(source, target);
   multiply(mat, r, g, b, outR, outG, outB);
  }

 private:
  static Matrix3x3 gamutToXYZ(Gamut g) {
   switch (g) {
    case Gamut::sRGB:
    case Gamut::Rec709:          return rec709ToXYZ();
    case Gamut::Rec2020:         return rec2020ToXYZ();
    case Gamut::DCI_P3:          return dciP3ToXYZ();
    case Gamut::DisplayP3:       return displayP3ToXYZ();
    case Gamut::ACES_AP0:        return acesAP0ToXYZ();
    case Gamut::ACES_AP1:        return acesAP1ToXYZ();
    case Gamut::AdobeRGB:        return adobeRGBToXYZ();
    case Gamut::XYZ_D65:         return identity();
    case Gamut::XYZ_D60:         return identity();
    case Gamut::DaVinciWideGamut: return dawgToXYZ();
    default:                     return identity();
   }
  }
  static Matrix3x3 xyzToGamut(Gamut g) {
   switch (g) {
    case Gamut::sRGB:
    case Gamut::Rec709:          return xyzToRec709();
    case Gamut::Rec2020:         return xyzToRec2020();
    case Gamut::DCI_P3:          return xyzToDCIP3();
    case Gamut::DisplayP3:       return xyzToDisplayP3();
    case Gamut::ACES_AP0:        return xyzToAcesAP0();
    case Gamut::ACES_AP1:        return xyzToAcesAP1();
    case Gamut::AdobeRGB:        return xyzToAdobeRGB();
    case Gamut::XYZ_D65:         return identity();
    case Gamut::XYZ_D60:         return identity();
    case Gamut::DaVinciWideGamut: return xyzToDawg();
    default:                     return identity();
   }
  }
 };

}
