#pragma once


#include <QtCore/QString>
#include <QtCore/QJsonObject>


namespace ArtifactCore {

 enum class eFramerateStringFormat {

 };
 enum class eFamousFrameRate {
  fps15_0,
  fps_24_0,
  fps29_7,
  fps30_0,
  fps60_0,
 };


 class FrameratePrivate;

 class Framerate {
 private:
  FrameratePrivate* const	pFrameRate_;
 public:
  Framerate();
  Framerate(float frameRate);
  Framerate(const QString& str);
  Framerate(const Framerate& frameRate);
  Framerate(Framerate&& framerate)noexcept;
  virtual ~Framerate();
  float framerate() const;
  void setFramerate(float frame = 30.0f);
  QString toString() const;
  void setFromString(const QString& framerate);
  bool hasDropframe() const;
  void speedUp(float frame = 1.0f);
  void speedDown(float frame = 1.0f);

  void swap(Framerate& other) noexcept;

  JsonObject toJson() const;
  void setFromJson(const JsonObject& object);
  void readFromJson(JsonObject& object) const;
  void writeToJson(JsonObject& object) const;

  Framerate& operator=(float rate);
  Framerate& operator=(const PString& str);
  Framerate& operator=(const Framerate& framerate);
  Framerate& operator=(Framerate&& framerate) noexcept;
 };

 bool operator==(const Framerate& framerate1, const Framerate& framerate2);
 bool operator!=(const Framerate& framerate1, const Framerate& framerate2);


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