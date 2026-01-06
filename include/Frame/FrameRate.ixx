module;


#include <QtCore/QString>
#include <QtCore/QJsonObject>
#include "../Define/DllExportMacro.hpp"

export module Frame.Rate;

import std;
import Utils.String.UniString;

export namespace ArtifactCore {

 enum class eFramerateStringFormat {

 };
 enum class eFamousFrameRate {
  fps15_0,
  fps_24_0,
  fps29_7,
  fps30_0,
  fps60_0,
 };

	
 class LIBRARY_DLL_API FrameRate final {
 private:
  class Impl;
  Impl* impl_;
 public:
  FrameRate();
  FrameRate(float frameRate);
  FrameRate(const QString& str);
  FrameRate(const FrameRate& frameRate);
  FrameRate(FrameRate&& framerate)noexcept;
  virtual ~FrameRate();
  float framerate() const;
  void setFrameRate(float frame = 30.0f);
  UniString toString() const;
  void setFromString(const QString& framerate);
  bool hasDropframe() const;
  void speedUp(float frame = 1.0f);
  void speedDown(float frame = 1.0f);

  void swap(FrameRate& other) noexcept;

  QJsonObject toJson() const;
  void setFromJson(const QJsonObject& object);
  void readFromJson(QJsonObject& object) const;
  void writeToJson(QJsonObject& object) const;

  UniString toDisplayString(bool includeDropframe = true) const; // UI向け表示
  static FrameRate fromJsonStatic(const QJsonObject& obj);

  FrameRate& operator=(float rate);
  FrameRate& operator=(const QString& str);
  FrameRate& operator=(const FrameRate& framerate);
  FrameRate& operator=(FrameRate&& framerate) noexcept;
 };

 bool operator==(const FrameRate& framerate1, const FrameRate& framerate2);
 bool operator!=(const FrameRate& framerate1, const FrameRate& framerate2);


 class FrameRateOffsetPrivate;

 class FrameRateOffset {
 private:

 public:
  explicit FrameRateOffset();
  ~FrameRateOffset();
 };

 bool operator==(const FrameRateOffset& offset1, const FrameRateOffset& offset2);
 bool operator!=(const FrameRateOffset& offset1, const FrameRateOffset& offset2);
 bool operator<=(const FrameRateOffset& offset1, const FrameRateOffset& offset2);








};