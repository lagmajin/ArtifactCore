module;


#include <QtCore/QString>
#include <QtCore/QJsonObject>

export module Frame.FrameRate;

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




 class Framerate final {
 private:
  class Impl;
  Impl* impl_;
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

  QJsonObject toJson() const;
  void setFromJson(const QJsonObject& object);
  void readFromJson(QJsonObject& object) const;
  void writeToJson(QJsonObject& object) const;

  QString toDisplayString(bool includeDropframe = true) const; // UI向け表示
  static Framerate fromJsonStatic(const QJsonObject& obj);

  Framerate& operator=(float rate);
  Framerate& operator=(const QString& str);
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