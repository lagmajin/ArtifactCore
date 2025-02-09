#pragma once

export module FloatRGBA;

export namespace ArtifactCore {

 class FloatColor;

 class FloatRGBAPrivate;

 class  FloatRGBA {
 private:
  FloatRGBAPrivate* pRGBA_;

  class FloatRGBAIterator {

  };

 public:
  FloatRGBA();
  explicit FloatRGBA(float r, float g, float b, float a);
  FloatRGBA(const FloatRGBA& rgba);
  FloatRGBA(FloatRGBA&& rgba);
  ~FloatRGBA();
  float r() const;
  float g() const;
  float b() const;
  float a() const;
  void setRed(float r);
  void setGreen(float g);
  void setBlue(float b);
  void setApha(float a);
  void setRGBA(float r, float g, float b = 0.0f, float a = 0.0f);

  void swap(FloatRGBA& other) noexcept;

  void setFromFloatColor(const FloatColor& color);
  void setFromRandom();

  //void readFromJson(const QJsonObject& object);
  //void writeFromJson(QJsonObject& object) const;

  operator FloatColor() const;

  float& operator[](int index);

  FloatRGBA& operator+(const FloatRGBA& rgba);

  FloatRGBA& operator=(const FloatColor& color);

  FloatRGBA& operator=(const FloatRGBA& rgba);
  FloatRGBA& operator=(FloatRGBA&& rgba);
 };


};