module;
#include "../Define/DllExportMacro.hpp"

export module Audio.Decibels;

import std;
import Utils.String.UniString;

export namespace ArtifactCore
{
 class LIBRARY_DLL_API AudioDecibels final
 {
 private:
  class Impl;
  Impl* impl_;

  static constexpr float EPSILON = 1e-6f;
  static constexpr float MIN_DB = -60.0f;
  static constexpr float MAX_DB = 20.0f;

 public:
  // コンストラクタ/デストラクタ
  AudioDecibels();
  explicit AudioDecibels(float dB);
  AudioDecibels(const AudioDecibels& other);
  AudioDecibels(AudioDecibels&& other) noexcept;
  ~AudioDecibels();

  // ゲッター/セッター
  float decibels() const;
  void setDecibels(float dB);

  // AudioVolumeとの相互変換
  float toLinearValue() const;
  static AudioDecibels fromLinearValue(float linear);

  // 補間（フェード用）
  AudioDecibels interpolate(const AudioDecibels& target, float t) const;
  static AudioDecibels linearFade(const AudioDecibels& from, const AudioDecibels& to, float t);

  // バリデーション
  bool isValid() const;
  bool isMute() const;
  bool isSilent() const;  // -40dB以下
  bool isNormalized() const;  // 0dB
  void clamp();

  // プリセット
  enum class Preset {
    Mute,          // -∞ dB
    VeryQuiet,     // -40 dB
    Quiet,         // -20 dB
    Normal,        // 0 dB
    Loud,          // +6 dB
    VeryLoud,      // +12 dB
    Maximum        // +20 dB
  };

  static AudioDecibels createPreset(Preset preset);
  static UniString getPresetName(Preset preset);

  // シリアライズ
  UniString serialize() const;
  static AudioDecibels deserialize(const UniString& data);

  // 比較（エプシロン考慮）
  bool equals(const AudioDecibels& other) const;
  bool operator==(const AudioDecibels& other) const;
  bool operator!=(const AudioDecibels& other) const;
  bool operator<(const AudioDecibels& other) const;
  bool operator<=(const AudioDecibels& other) const;
  bool operator>(const AudioDecibels& other) const;
  bool operator>=(const AudioDecibels& other) const;

  // 演算子（デシベル計算用）
  AudioDecibels operator+(float dB) const;
  AudioDecibels operator-(float dB) const;
  AudioDecibels operator*(float scalar) const;
  AudioDecibels operator/(float scalar) const;

  AudioDecibels& operator+=(float dB);
  AudioDecibels& operator-=(float dB);
  AudioDecibels& operator*=(float scalar);
  AudioDecibels& operator/=(float scalar);

  // 代入演算子
  AudioDecibels& operator=(const AudioDecibels& other);
  AudioDecibels& operator=(AudioDecibels&& other) noexcept;
 };

};
