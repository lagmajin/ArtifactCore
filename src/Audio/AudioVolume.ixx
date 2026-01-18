module;
#include "../Define/DllExportMacro.hpp"

export module Audio.Volume;

import std;
import Utils.String.UniString;

namespace ArtifactCore
{
 class LIBRARY_DLL_API AudioVolume final
 {
 private:
  class Impl;
  Impl* impl_;

  static constexpr float EPSILON = 1e-6f;

 public:
  // コンストラクタ/デストラクタ
  AudioVolume();
  explicit AudioVolume(float volume);
  AudioVolume(const AudioVolume& volume);
  AudioVolume(AudioVolume&& other) noexcept;
  ~AudioVolume();

  // ゲッター/セッター
  float volume() const;
  void setVolume(float v);

  // dB変換
  float toDecibels() const;                    // リニア → dB
  static AudioVolume fromDecibels(float dB);  // dB → リニア

  // 補間（フェード用）
  AudioVolume interpolate(const AudioVolume& target, float t) const;
  static AudioVolume linearFade(const AudioVolume& from, const AudioVolume& to, float t);

  // バリデーション
  bool isValid() const;
  bool isMuted() const;
  bool isNormalized() const;
  void clamp();

  // プリセット
  enum class Preset {
    Mute,
    VeryQuiet,
    Quiet,
    Normal,
    Loud,
    Maximum
  };

  static AudioVolume createPreset(Preset preset);
  static UniString getPresetName(Preset preset);

  // シリアライズ
  UniString serialize() const;
  static AudioVolume deserialize(const UniString& data);

  // 比較（エプシロン考慮）
  bool equals(const AudioVolume& other) const;
  bool operator==(const AudioVolume& other) const;
  bool operator!=(const AudioVolume& other) const;
  bool operator<(const AudioVolume& other) const;
  bool operator<=(const AudioVolume& other) const;
  bool operator>(const AudioVolume& other) const;
  bool operator>=(const AudioVolume& other) const;

  // 演算子
  AudioVolume operator+(const AudioVolume& other) const;
  AudioVolume operator-(const AudioVolume& other) const;
  AudioVolume operator*(float scalar) const;
  AudioVolume operator/(float scalar) const;

  AudioVolume& operator+=(const AudioVolume& other);
  AudioVolume& operator-=(const AudioVolume& other);
  AudioVolume& operator*=(float scalar);
  AudioVolume& operator/=(float scalar);

  // 代入演算子
  AudioVolume& operator=(const AudioVolume& volume);
  AudioVolume& operator=(AudioVolume&& other) noexcept;
 };

};
